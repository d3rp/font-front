#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "text.h"
#include "shader.h"

static const char* shader_string_for_vertices = R"SHADER_INPUT(
#version 330 core
layout (location = 0) in vec4 vertex; // pos, tex
out vec2 TexCoords;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)SHADER_INPUT";

static const char* shader_string_for_fragments = R"SHADER_INPUT(
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;
uniform vec3 textColor;

void main()
{
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
}
)SHADER_INPUT";

namespace spec
{
constexpr int atlas_texture_w = 1024;
constexpr int atlas_texture_h = 1024;

namespace vertex_data
{
constexpr int triangle_points_n = 6;
constexpr int pos_points_n      = 4;
} // namespace vertex_data
} // namespace spec

namespace typesetting
{

struct VertexDataFormat
{
    using base_t = float;
    base_t data[spec::vertex_data::triangle_points_n][spec::vertex_data::pos_points_n];
};

void print_glyph_infos(const hb_glyph_info_t* glyphs, unsigned int length)
{
    for (unsigned int i = 0; i < length; ++i)
    {
        printf("Glyph %u:\n", i);
        printf("\tCodepoint: %u\n", glyphs[i].codepoint);
        printf("\tMask: %u\n", glyphs[i].mask);
        printf("\tCluster: %u\n", glyphs[i].cluster);
        printf("\tVar1: %d\n", glyphs[i].var1.i32);
        printf("\tVar2: %d\n", glyphs[i].var2.i32);
    }
}

void print_glyph_positions(const hb_glyph_position_t* positions, unsigned int length)
{
    for (unsigned int i = 0; i < length; ++i)
    {
        printf("Position %u:\n", i);
        printf("\tX Advance: %d\n", positions[i].x_advance);
        printf("\tY Advance: %d\n", positions[i].y_advance);
        printf("\tX Offset: %d\n", positions[i].x_offset);
        printf("\tY Offset: %d\n", positions[i].y_offset);
        printf("\tVar: %d\n", positions[i].var.i32);
    }
}

// Wrapper for text to pass for Text Renderer
template <typename VertexDataType>
struct RenderableText : HarfbuzzAdaptor
{
    explicit RenderableText(const Text& text_ref, const Font& font_ref, const UnicodeTranslation& translation_ref)
        : text(text_ref)
        , font(font_ref)
        , dirty(true)
    {
        translation = translation_ref;
    }

    void update()
    {
        if (!dirty)
            return;

        glyph_infos.clear();
        // TODO: can we init-allocate this..
        Buffer* buffer = hb_buffer_create();
        on_scope_exit([&buffer] { hb_buffer_destroy(buffer); });

        hb_buffer_add_utf8(buffer, text.text.c_str(), -1, 0, -1);

        // If you don't know the direction, script, and language
        //        hb_buffer_guess_segment_properties(buffer);

        hb_buffer_set_direction(buffer, translation.direction);
        hb_buffer_set_script(buffer, translation.script);
        hb_buffer_set_language(buffer, translation.language);

        // beef
        hb_shape(font.unicode, buffer, nullptr, 0);

        unsigned int glyph_n;
        auto* glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_n);
        auto* glyph_pos  = hb_buffer_get_glyph_positions(buffer, &glyph_n);

        print_glyph_infos(glyph_info, glyph_n);
        print_glyph_positions(glyph_pos, glyph_n);

        for (uint i = 0; i < glyph_n; ++i)
            glyph_infos.emplace_back(std::forward<GlyphInfo>(GlyphInfo {
                glyph_info[i].codepoint,
                glyph_pos[i].x_offset / 64,
                glyph_pos[i].y_offset / 64,
                glyph_pos[i].x_advance / 8,
                glyph_pos[i].y_advance / 64 }));

        dirty = false;
    }

    const Text& text;
    const Font& font;

    std::vector<GlyphInfo> glyph_infos;
    mutable bool dirty = true;
    mutable std::vector<VertexDataType> vertices {};
};

// Passes shit to the shader
struct TextRenderer
    : blueprints::InitDestroy<TextRenderer>
    , blueprints::Commitable<TextRenderer>
{
    struct
    {
        bool init(int textures_n, int def_max_quads)
        {
            assert(textures_n > 0 && textures_n <= 16);
            assert(def_max_quads > 0 && def_max_quads <= 1024);

            p.max_quads = def_max_quads;

            std::string errorLog;
            if (!p.program.Init(shader_string_for_vertices, shader_string_for_fragments, errorLog))
                return false;

            glGenVertexArrays(1, &p.vao);
            glGenBuffers(1, &p.vbo);
            glBindVertexArray(p.vao);
            glBindBuffer(GL_ARRAY_BUFFER, p.vbo);
            glBufferData(GL_ARRAY_BUFFER, p.max_quads * sizeof(VertexDataFormat), nullptr, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(
                0,
                spec::vertex_data::pos_points_n,
                GL_FLOAT,
                GL_FALSE,
                spec::vertex_data::pos_points_n * sizeof(VertexDataFormat::base_t),
                0
            );
            glBindVertexArray(0);
            for (int i = 0; i < textures_n; i++)
            {
                std::unique_ptr<Atlas> t = std::make_unique<Atlas>();
                if (!t->init(spec::atlas_texture_w, spec::atlas_texture_h))
                    return false;

                p.atlases.emplace_back(std::move(t));
                p.dyn_atlases.push_back(0);
            }

            p.line.tex_index = -1;

            p.vertices = (float*) malloc(p.max_quads * sizeof(VertexDataFormat));
            if (!p.vertices)
                return false;

            return true;
        }

        bool destroy()
        {
            glDeleteBuffers(1, &p.vbo);
            glDeleteVertexArrays(1, &p.vao);
            free(p.vertices);

            return true;
        }

        bool begin(int w, int h)
        {
            glEnable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            p.program.Use(true);

            glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(w), 0.0f, static_cast<float>(h));
            glUniformMatrix4fv(p.program.GetUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glBindVertexArray(p.vao);
            glActiveTexture(GL_TEXTURE0);

            return true;
        }

        bool commit()
        {
            if (!p.cur_quad)
                return false;

            // update content of VBO memory
            glBufferSubData(GL_ARRAY_BUFFER, 0, p.cur_quad * sizeof(VertexDataFormat), p.vertices);
            // render quad
            glDrawArrays(GL_TRIANGLES, 0, p.cur_quad * spec::vertex_data::triangle_points_n);

            p.cur_quad = 0;

            return true;
        }

        TextRenderer& p;
    } impl { *this };

    void end()
    {
        impl.commit();
        glBindVertexArray(0);
        program.Use(false);
    }

    void set_colour(Colour c)
    {
        if (last_colour != c)
            commit();

        glUniform3f(program.GetUniformLocation("textColor"), c.x, c.y, c.z);
        last_colour = c;
    }

    // adds and update glyph (returns revised version)
    Glyph add_to_atlas(Glyph& glyph, const uint8_t* data)
    {
        auto [width, height, tex_x, tex_y]
            = std::tuple { glyph.size.x, glyph.size.y, glyph.tex_index, glyph.dyn_tex };

        for (size_t i = 0; i < atlases.size(); ++i)
        {
            auto* atlas = atlases[i].get();
            if (atlas->add_region(glyph, data))
            {
                Glyph new_glyph     = glyph;
                new_glyph.tex_index = i;
                new_glyph.dyn_tex   = dyn_atlases[i];
                return new_glyph;
            }
        }

        // evict a random choosed one
        const auto index = (size_t) rand() % atlases.size();
        auto* atlas      = atlases[index].get();
        atlas->clear();
        dyn_atlases[index]++;
        textures_evicted++;

        // retry
        if (atlas->add_region(glyph, data))
        {
            Glyph new_glyph     = glyph;
            new_glyph.tex_index = index;
            new_glyph.dyn_tex   = dyn_atlases[index];
            return new_glyph;
        }

        throw std::runtime_error("Failed to add region for glyph");
    }

    std::optional<std::reference_wrapper<Glyph>> get_glyph(const Font& font, unsigned int glyph_index)
    {
        GlyphKey key { font.id, glyph_index };
        auto iter = glyphs.find(key);
        // glyph exists in cache
        if (iter != glyphs.end())
        {
            auto& glyph = iter->second;
            if (glyph.tex_index >= 0 && (glyph.dyn_tex == dyn_atlases[glyph.tex_index]))
            {
                textures_required++;
                textures_hit++;
            }
            return { glyph };
        }

        // glyph needs to be created
        auto face = font.face;
        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)
            || FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL))
            throw std::runtime_error("Glyph load failed at: " + std::to_string(glyph_index));

        int tex_index         = -1;
        unsigned int dyn_tex  = 0;
        uint16_t tex_offset_x = 0, tex_offset_y = 0;
        auto* g = face->glyph;
        // Create placeholder
        auto glyph = Glyph { glm::ivec2 { g->bitmap.width, g->bitmap.rows },
                             glm::ivec2 { g->bitmap_left, g->bitmap_top },
                             glm::ivec2 { tex_offset_x, tex_offset_y },
                             tex_index,
                             dyn_tex };
        // update atlas and update its info to the glyph
        if (g->bitmap.width > 0 && g->bitmap.rows > 0)
        {
            glyph = add_to_atlas(glyph, g->bitmap.buffer);
            textures_required++;
        }

        glyphs[key] = glyph;

        return { glyphs.at(key) };
    }

    void set_tex_id(unsigned int tex_id)
    {
        if (tex_id != last_tex_id)
            commit();

        glBindTexture(GL_TEXTURE_2D, tex_id);
        last_tex_id = tex_id;
    }

    void append_quad(VertexDataFormat v)
    {
        if (cur_quad == max_quads)
            commit();

        assert(cur_quad < max_quads);
        memcpy(
            vertices + cur_quad * spec::vertex_data::triangle_points_n * spec::vertex_data::pos_points_n,
            v.data,
            sizeof(VertexDataFormat)
        );
        cur_quad++;
    }

    template <typename VertexDataType>
    void draw_text(RenderableText<VertexDataType>& renderable, const Point p, const Colour colour)
    {
        set_colour(colour);
        renderable.update();

        for (auto& info : renderable.glyph_infos)
        {
            auto g_opt = get_glyph(renderable.font, info.codepoint);
            Glyph g    = std::invoke(
                [&]
                {
                    if (!g_opt)
                        throw std::runtime_error("get glyph error");
                    else
                        return g_opt.value();
                }
            );
            auto g_w = g.size.x, g_h = g.size.y;
            if (g_w > 0 && g_h > 0)
            {
                auto* atlas = atlases[g.tex_index].get();
                set_tex_id(atlas->texture);

                float glyph_x = p.x + g.bearing.x + info.x_offset;
                float glyph_y = p.y - (g.size.y - g.bearing.y) + info.y_offset;
                auto glyph_w  = (float) g.size.x;
                auto glyph_h  = (float) g.size.y;

                float tex_x = g.tex_offset.x / (float) atlas->width;
                float tex_y = g.tex_offset.y / (float) atlas->height;
                float tex_w = glyph_w / (float) atlas->width;
                float tex_h = glyph_h / (float) atlas->height;

                // update VBO for each glyph
                append_quad({ { { glyph_x, glyph_y + glyph_h, tex_x, tex_y },
                                { glyph_x, glyph_y, tex_x, tex_y + tex_h },
                                { glyph_x + glyph_w, glyph_y, tex_x + tex_w, tex_y + tex_h },

                                { glyph_x, glyph_y + glyph_h, tex_x, tex_y },
                                { glyph_x + glyph_w, glyph_y, tex_x + tex_w, tex_y + tex_h },
                                { glyph_x + glyph_w, glyph_y + glyph_h, tex_x + tex_w, tex_y } } });
            }
        }
    }

    void print_stats()
    {
        fprintf(stdout, "\n");
        fprintf(stdout, "----glyph texture cache stats----\n");
        if (!atlases.empty())
        {
            const auto& atlas = *atlases[0]; // assuming all atlases have the same size
            fprintf(stdout, "texture atlas size: %d %d\n", atlas.width, atlas.height);
        }
        else
        {
            fprintf(stdout, "no texture atlases created\n");
        }
        fprintf(stdout, "texture atlas count: %zu\n", atlases.size());
        fprintf(stdout, "texture atlas occupancy: ");
        for (const auto& atlas : atlases)
        {
            float rate = atlas->bin_packer.Occupancy() * 100.f;
            fprintf(stdout, " %.1f%%", rate);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "texture atlas evict: %llu\n", textures_evicted);
        fprintf(stdout, "request: %llu\n", textures_required);
        fprintf(stdout, "hit    : %llu (%.2f%%)\n", textures_hit, static_cast<double>(textures_hit) / textures_required * 100);
        fprintf(stdout, "\n");
    }

    VertexDataFormat::base_t* vertices;

    // vertex array and vertex buffer objects
    GLuint vao, vbo;
    Colour last_colour;


    using Atlases        = std::vector<std::unique_ptr<Atlas>>;
    Atlases atlases;
    using DynamicAtlases = std::vector<unsigned int>;
    DynamicAtlases dyn_atlases;

    GlyphCache glyphs;
    Glyph line;

    ShaderProgram program;

    unsigned int last_tex_id   = 0;
    uint64_t textures_required = 0;
    uint64_t textures_hit      = 0;
    uint64_t textures_evicted  = 0;

    GLsizeiptr max_quads;
    GLsizeiptr cur_quad;
};

// I'd prefer to organise this way, but it's a lot of hassle for a simple demo
//      template <typename T>
//      struct Renderer { ... };
//
//      template <>
//      struct Renderer<Text>
//      {
//
//      };

} // namespace typesetting