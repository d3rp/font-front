#pragma once

namespace typesetting
{
using GlyphKey = std::pair<unsigned int, FT_UInt>;

struct Glyph
{
    glm::ivec2 size { 0, 0 };       // Size of glyph
    glm::ivec2 bearing { 0, 0 };    // Offset from horizontal layout origin to left/top of glyph
    glm::ivec2 tex_offset { 0, 0 }; // Offset of glyph in texture atlas
    int tex_index        = -1;      // Texture atlas index
    unsigned int dyn_tex = 0;       // Texture atlas generation
};

struct GlyphKeyHash
{
    size_t operator()(const GlyphKey& key) const
    {
        size_t hash1 = std::hash<unsigned int>()(key.first);
        size_t hash2 = std::hash<FT_UInt>()(key.second);
        return hash1 ^ (hash2 << 1);
    }
};

struct GlyphKeyEqual
{
    bool operator()(const GlyphKey& lhs, const GlyphKey& rhs) const
    {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
};

using GlyphCache = std::unordered_map<GlyphKey, Glyph, GlyphKeyHash, GlyphKeyEqual>;

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

} // namespace typesetting