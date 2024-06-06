#include <iostream>
#include <functional>
#include <filesystem>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "scope_guards.h"
#include "text.h"
#include "rendering.h"

namespace spec
{
constexpr int w = 800;
constexpr int h = 600;
} // namespace spec

static void print_versions(typesetting::Resources& resources)
{
    auto print_opengl_version = [&]{
        printf("OpenGL version: %d.%d\n", GLVersion.major, GLVersion.minor);
    };
    auto print_freetype_version = [&] {
        FT_Int major, minor, patch;
        FT_Library_Version(resources.library, &major, &minor, &patch);
        printf("FreeType version: %d.%d.%d\n", major, minor, patch);
    };

    printf("GLFW version: %s\n", glfwGetVersionString());
    print_freetype_version();
    print_opengl_version();
    printf("HarfBuzz version: %s\n", hb_version_string());
}
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

    auto* window = glfwCreateWindow(spec::w, spec::h, "foobar", nullptr, nullptr);
    check_failed(window, "Create window failed");

    glfwSetWindowRefreshCallback(
        window,
        [](auto* window)
        {
            // draw(window);
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
    Resources resources;
    {
        bool status = resources.init();
        check_failed(status, "Resources init failed"); // 4 fonts and 256 max chars?
    }
    on_scope_exit([&] { resources.destroy(); });

    print_versions(resources);

    TextRenderer rdr;
    {
        bool status = rdr.init(4, 256);
        check_failed(status, "Renderer init failed"); // 4 fonts and 256 max chars?
    }
    on_scope_exit([&] { rdr.destroy(); });

    const std::string font_dir  = FONTS_DIR;

    auto add_font = [&resources,content_scale](const std::string font_path){
        Font font;
        if (!std::filesystem::exists(font_path))
            throw std::runtime_error("Error: Font file not found");

        auto _ = create_font(&resources, font_path.c_str(), 56, content_scale);
        if (_)
            font = _.value();
        else
            throw std::runtime_error("font failed");
        return font;
    };
    auto font_latin = add_font(font_dir + "/NotoSans-Regular.ttf");
    on_scope_exit([&] { destroy_font(font_latin); });
    auto font_arabic = add_font(font_dir + "/NotoSansArabic-Regular.ttf");
    on_scope_exit([&] { destroy_font(font_arabic); });


    Text text_latin { 0, colours::red, {}, u8"abcdefghijklmnopqrstuvxyz" };
//    Text text_arabic { 0, colours::red, {}, u8"أ" };
    Text text_arabic { 0, colours::red, {}, u8"أسئلة و أجوبة" };
    HarfbuzzAdaptor::UnicodeTranslation tr_latin { HB_DIRECTION_LTR,
                                                   HB_SCRIPT_LATIN,
                                                   hb_language_from_string("en", -1) };
    HarfbuzzAdaptor::UnicodeTranslation tr_arabic { HB_DIRECTION_RTL,
                                                   HB_SCRIPT_ARABIC,
                                                   hb_language_from_string("ar", -1) };
    RenderableText<VertexDataFormat> latin(text_latin, font_latin, tr_latin);
    RenderableText<VertexDataFormat> arabic(text_arabic, font_arabic, tr_arabic);

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
        rdr.draw_text(latin, { DP_X(10.0f * content_scale), DP_Y(60.0f * content_scale) }, colours::red);
        rdr.draw_text(arabic, { DP_X(300.0f * content_scale), DP_Y(300.0f * content_scale) }, colours::green);
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
