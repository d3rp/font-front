#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <unicode/ubidi.h>
#include <unicode/unistr.h>

#include "spec.h"
#include "text.h"
#include "shader.h"
#include "skyline_binpack.h"
#include "utlz.h"

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
    float y = texture(text, TexCoords).r;
    if (y < 0.5)
        discard;
    y *= 1.45;
    y = 1.0 / (1.0 + exp(-20.0 * (y - 0.76)));

    vec4 sampled = vec4(1.0, 1.0, 1.0, y);
    color = vec4(textColor, 1.0) * sampled;
}
)SHADER_INPUT";

namespace typesetting
{

struct VertexDataFormat
{
    using base_t = float;
    base_t data[spec::vertex_data::triangle_points_n][spec::vertex_data::pos_points_n];
};

// Wrapper for text to pass for Text Renderer
template <typename VertexDataType>
struct RenderableText
{
    explicit RenderableText(const Text& text_ref, const Font& font_ref, const Colour c = colours::red)
        : text(text_ref)
        , font(font_ref)
        , dirty(true)
        , colour(c)
    {
    }

    void update()
    {
        if (!dirty)
            return;

        glyph_infos.clear();
        // TODO: can we init-allocate this..
        //        auto* buffer = hb_buffer_create();
        //        on_scope_exit([&buffer] { hb_buffer_destroy(buffer); });

        auto [hb_infos, hb_poss, n] { shaper.shape(text, font) };
        for (uint i = 0; i < n; ++i)
            glyph_infos.emplace_back(std::forward<hb_helpers::GlyphInfo>(
                { hb_infos[i].codepoint,
                  hb_poss[i].x_offset / 64,
                  hb_poss[i].y_offset / 64,
                  hb_poss[i].x_advance / 64,
                  hb_poss[i].y_advance / 64 }
            ));

        dirty = false;
    }

    hb_helpers::Shaper shaper;
    std::vector<hb_helpers::GlyphInfo> glyph_infos;
    const Text& text;
    const Font& font;
    Colour colour;

    mutable bool dirty = true;
};

using GlyphKey = std::pair<unsigned int, unsigned int>;

struct Glyph
{
    glm::ivec2 size { 0, 0 };       // Size of glyph
    glm::ivec2 bearing { 0, 0 };    // Offset from horizontal layout origin to left/top of glyph
    glm::ivec2 tex_offset { 0, 0 }; // Offset of glyph in texture atlas
    int tex_index        = -1;      // Texture atlas index
    unsigned int dyn_tex = 0;       // Texture atlas generation
};

using GlyphCache = std::unordered_map<GlyphKey, Glyph>;

struct Atlas
{
    bool init(uint16_t w, uint16_t h)
    {
        assert(w > 0);
        assert(h > 0);

        width  = w;
        height = h;

        bin_packer.init(w, h);

        // TODO: uuwee
        data = (uint8_t*) calloc(w * h * 1, sizeof(uint8_t));
        if (!data)
            return false;

        // generate texture
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

        // set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        return true;
    }

    bool destroy()
    {
        free(data);
        glDeleteTextures(1, &texture);
        return true;
    }

    void clear()
    {
        assert(width > 0);
        assert(height > 0);
        assert(data != nullptr);
        assert(texture != 0);

        bin_packer.init(width, height);

        memset(data, 0, width * height * 1 * sizeof(uint8_t));

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);
    };

    std::optional<Point> add_region(const Glyph& glyph, const uint8_t* glyph_data)
    {
        auto glyph_w = glyph.size.x, glyph_h = glyph.size.y;
        assert(glyph_w > 0);
        assert(glyph_h > 0);
        assert(glyph_data != nullptr);

        assert(width > 0);
        assert(height > 0);
        assert(data != nullptr);
        assert(texture != 0);

        auto rect = bin_packer.insert(glyph_w, glyph_h);
        if (rect.height <= 0)
            return std::nullopt;

        for (uint16_t i = 0; i < glyph_h; ++i)
            memcpy(data + ((rect.y + i) * width + rect.x), glyph_data + i * glyph_w, glyph_w);

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, glyph_w, glyph_h, GL_RED, GL_UNSIGNED_BYTE, glyph_data);
        glBindTexture(GL_TEXTURE_2D, 0);

        return { { rect.x, rect.y } };
    }

    uint16_t width {};
    uint16_t height {};
    binpack::SkylineBinPack bin_packer;
    uint8_t* data {};
    unsigned int texture {};
};

// Passes shit to the shader
struct TextRenderer
{
    bool init(int textures_n, int def_max_quads)
    {
        assert(textures_n > 0 && textures_n <= 16);
        assert(def_max_quads > 0 && def_max_quads <= 1024);

        max_quads = def_max_quads;

        std::string errorLog;
        if (!program.init(shader_string_for_vertices, shader_string_for_fragments, errorLog))
            return false;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, max_quads * sizeof(VertexDataFormat), nullptr, GL_DYNAMIC_DRAW);
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

            atlases.emplace_back(std::move(t));
            dyn_atlases.push_back(0);
        }

        line.tex_index = -1;

        vertices = (float*) malloc(max_quads * sizeof(VertexDataFormat));
        if (!vertices)
            return false;

        return true;
    }

    bool destroy()
    {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        free(vertices);

        return true;
    }

    bool begin(int w, int h)
    {
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        program.use(true);

        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(w), 0.0f, static_cast<float>(h));
        glUniformMatrix4fv(program.get_uniform_location("projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);

        return true;
    }

    bool commit()
    {
        if (!cur_quad)
            return false;
        glDepthMask(GL_FALSE); // Don't write into the depth buffer
        // update content of VBO memory
        glBufferSubData(GL_ARRAY_BUFFER, 0, cur_quad * sizeof(VertexDataFormat), vertices);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, cur_quad * spec::vertex_data::triangle_points_n);
        glDepthMask(GL_TRUE); // Don't write into the depth buffer

        cur_quad = 0;

        return true;
    }

    void end()
    {
        commit();
        glBindVertexArray(0);
        program.use(false);
    }

    void set_colour(Colour c)
    {
        if (last_colour != c)
            commit();

        glUniform3f(program.get_uniform_location("textColor"), c.x, c.y, c.z);
        last_colour = c;
    }

    void set_tex_id(unsigned int tex_id)
    {
        if (tex_id != last_tex_id)
            commit();

        glBindTexture(GL_TEXTURE_2D, tex_id);
        last_tex_id = tex_id;
    }

    // adds and update glyph (returns revised version)
    Glyph add_to_atlas(Glyph glyph, const uint8_t* data)
    {
        for (size_t i = 0; i < atlases.size(); ++i)
        {
            auto* atlas = atlases[i].get();
            if (auto tex_offset_opt = atlas->add_region(glyph, data))
            {
                Glyph new_glyph      = glyph;
                new_glyph.tex_index  = i;
                new_glyph.dyn_tex    = dyn_atlases[i];
                new_glyph.tex_offset = tex_offset_opt.value();
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
        if (auto tex_offset_opt = atlas->add_region(glyph, data))
        {
            Glyph new_glyph      = glyph;
            new_glyph.tex_index  = index;
            new_glyph.dyn_tex    = dyn_atlases[index];
            new_glyph.tex_offset = tex_offset_opt.value();
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

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction

        // glyph needs to be created
        auto face = font.face;
        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT))
            throw std::runtime_error("Glyph load failed at: " + std::to_string(glyph_index));

        // TODO: use SDF
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF))
            throw std::runtime_error("Glyph render failed at: " + std::to_string(glyph_index));

        auto* g = face->glyph;
        // Create placeholder
        Glyph glyph;
        // update atlas and update its info to the glyph
        if (g->bitmap.width > 0 && g->bitmap.rows > 0)
        {
            glyph.size    = { g->bitmap.width, g->bitmap.rows };
            glyph.bearing = { g->bitmap_left, g->bitmap_top };
            glyph         = add_to_atlas(glyph, g->bitmap.buffer);
            textures_required++;
        }

        glyphs[key] = glyph;

        return { glyphs.at(key) };
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

    std::vector<ShaperChunk> collect_chunks(
        std::vector<char32_t>& uchrs,
        std::vector<hb_helpers::FontShaper>& shapers,
        int shaper_depth,
        int chunk_start           = 0,
        int chunk_end             = -1,
        int fallback_shader_index = 0
    )
    {
        UBiDi* bidi = ubidi_open();

        static std::vector<ShaperChunk> chunks;
        if (shaper_depth == 0)
        {
            chunks.clear();
            chunks.reserve(uchrs.size() / 2); // Newton ayeah = good enough approximation
        }

        auto create_chunk = [&](auto start, auto end, auto depth)
        {
            ShaperChunk chunk;
            chunk.text_ptr     = &uchrs;
            chunk.range.start  = start;
            chunk.range.end    = end;
            chunk.range.length = end - start;
            chunk.shaper       = &shapers[depth];
            return chunk;
        };

        // Use fallback font when all font options have been tried
        if (shaper_depth == shapers.size())
        {
            //            if (auto chunk_opt = create_chunk(uchrs.size(), fallback_shader_index))
            //                chunks.emplace_back(std::forward<ShaperChunk>(chunk_opt.value()));

            return chunks;
        }

        bool should_collect = false;
        int new_start       = chunk_start;
        int new_end         = chunk_start;
        while (new_end < (chunk_end < 0 ? uchrs.size() : chunk_end))
        {
            if (is_char_supported(shapers[shaper_depth].font, uchrs[new_end]))
            {
                // Continue as long as this font applies to the characters
                ++new_end;
                should_collect = true;
            }
            else
            {
                // Save last iterated chunk with appropriate font
                if (should_collect)
                {
                    should_collect = false;
                    chunks.emplace_back(
                        std::forward<ShaperChunk>(create_chunk(new_start, new_end, shaper_depth))
                    );
                }

                // Look up how long the mismatching character set continues
                new_start = new_end;
                while (new_end < uchrs.size()
                       && (!is_char_supported(shapers[shaper_depth].font, uchrs[new_end])))
                {
                    ++new_end;
                    // skip over one character, if that also isn't supported by this shaper
                    // This matters apparently when spaces are introduced in specific shaping specs,
                    // e.g. arabic: "أسئلة و أجوبة And trying to render the spaces as latin chars in
                    // between will crash freetype load glyph
                    if (new_end + 1 < uchrs.size()
                        && !is_char_supported(shapers[shaper_depth].font, uchrs[new_end + 1]))
                        ++new_end;
                }

                // Recurssively try other shaders
                collect_chunks(uchrs, shapers, shaper_depth + 1, new_start, new_end);
                new_start = new_end;
            }
            //            std::cout << "iteration end at depth: " << shaper_depth << " new_start: "
            //            << new_start
            //                      << " new_end: " << new_end << "\n";
        }
        // Save last iterated chunk with appropriate font
        if (should_collect)
            chunks.emplace_back(std::forward<ShaperChunk>(create_chunk(new_start, new_end, shaper_depth)));

        return chunks;
    }

    template <typename VertexDataType>
    void draw_runs(std::vector<hb_helpers::ShaperRun>& runs, Point o, Colour colour)
    {
        for (auto& run : runs)
        {
            static std::vector<hb_helpers::GlyphInfo> glyph_infos;
            glyph_infos.clear();

            auto [hb_infos, hb_poss, n] = run.glyph_data();
            for (uint i = 0; i < n; ++i)
                glyph_infos.emplace_back(std::forward<hb_helpers::GlyphInfo>(
                    { hb_infos[i].codepoint,
                      hb_poss[i].x_offset / 64,
                      hb_poss[i].y_offset / 64,
                      hb_poss[i].x_advance / 64,
                      hb_poss[i].y_advance / 64 }
                ));

            for (auto& info : glyph_infos)
            {
                auto g_opt = get_glyph(*run.font, info.codepoint);
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

                    float glyph_x = o.x + g.bearing.x + info.x_offset;
                    float glyph_y = o.y - (g.size.y - g.bearing.y) + info.y_offset;
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

                o.x += info.x_advance;
                o.y += info.y_advance;
            }
        }
    }
    template <typename VertexDataType>
    void draw_text_yes(std::string& text, std::vector<hb_helpers::FontShaper>& shapers, Point o)
    {
        static std::vector<hb_helpers::GlyphInfo> glyph_infos;
        glyph_infos.clear();

        int i = 0;
        utf::string u_str(text.c_str());
        auto u_chrs = u_str.as_unicode();

        auto chunks = collect_chunks(u_chrs, shapers, 0);

        for (auto& chunk : chunks)
        {
            std::cout << to_string(chunk);

            auto [hb_infos, hb_poss, n] { chunk.shaper->shape(
                reinterpret_cast<const uint32_t*>(u_chrs.data()),
                u_chrs.size(),
                chunk.range.start,
                chunk.range.length
            ) };
            for (uint i = 0; i < n; ++i)
                glyph_infos.emplace_back(std::forward<hb_helpers::GlyphInfo>(
                    { hb_infos[i].codepoint,
                      hb_poss[i].x_offset / 64,
                      hb_poss[i].y_offset / 64,
                      hb_poss[i].x_advance / 64,
                      hb_poss[i].y_advance / 64 }
                ));

            for (auto& info : glyph_infos)
            {
                auto g_opt = get_glyph(chunk.shaper->font, info.codepoint);
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

                    float glyph_x = o.x + g.bearing.x + info.x_offset;
                    float glyph_y = o.y - (g.size.y - g.bearing.y) + info.y_offset;
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

                o.x += info.x_advance;
                o.y += info.y_advance;
            }
        }
    }

    template <typename VertexDataType>
    void draw_text(RenderableText<VertexDataType>& renderable, Point o)
    {
        draw_text(renderable, o, renderable.colour);
    }

    template <typename VertexDataType>
    void draw_text(RenderableText<VertexDataType>& renderable, Point o, const Colour colour)
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

                float glyph_x = o.x + g.bearing.x + info.x_offset;
                float glyph_y = o.y - (g.size.y - g.bearing.y) + info.y_offset;
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

            o.x += info.x_advance;
            o.y += info.y_advance;
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
            float rate = atlas->bin_packer.occupancy() * 100.f;
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

    using Atlases = std::vector<std::unique_ptr<Atlas>>;
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

} // namespace typesetting