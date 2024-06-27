#pragma once

#include <cuchar>

#define LOG_FUNC std::cout << __PRETTY_FUNCTION__ << "\n";

// Hash function for GlyphKey
template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
template <>
struct hash<pair<unsigned int, unsigned int>>
{
    size_t operator()(const pair<unsigned int, unsigned int>& p) const
    {
        size_t seed = 0;
        ::hash_combine(seed, p.first);
        ::hash_combine(seed, p.second);
        return seed;
    }
};
} // namespace std

namespace utlz
{
template <typename T>
class Iterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = T*; // or also value_type*
    using reference         = T&; // or also value_type&

    Iterator(pointer ptr)
        : m_ptr(ptr)
    {
    }

    reference operator*() const { return *m_ptr; }

    pointer operator->() { return m_ptr; }

    Iterator& operator++()
    {
        m_ptr++;
        return *this;
    }

    Iterator operator++(int)
    {
        Iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; }

    friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; }

private:
    pointer m_ptr;
};

float to_float(FT_F26Dot6 weird_number) { return float(weird_number) * 1.0f / 64.0f; }

FT_F26Dot6 to_ft_float(float value) { return FT_F26Dot6(value * 64.0f); }

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

static void print_versions(typesetting::Library& library)
{
    auto print_opengl_version = [&]
    { printf("OpenGL version: %d.%d\n", GLVersion.major, GLVersion.minor); };
    auto print_freetype_version = [&]
    {
        FT_Int major, minor, patch;
        FT_Library_Version(library.get(), &major, &minor, &patch);
        printf("FreeType version: %d.%d.%d\n", major, minor, patch);
    };

    printf("GLFW version: %s\n", glfwGetVersionString());
    print_freetype_version();
    print_opengl_version();
    printf("HarfBuzz version: %s\n", hb_version_string());
}

// Helper function to convert UTF-8 to UTF-32
std::u32string utf8_to_utf32(const std::string& utf8) {
    std::u32string utf32;
    char32_t ch = 0;
    int bytes = 0;
    for (unsigned char c : utf8) {
        if (bytes == 0) {
            if ((c & 0x80) == 0) {
                ch = c;
                bytes = 1;
            } else if ((c & 0xE0) == 0xC0) {
                ch = c & 0x1F;
                bytes = 2;
            } else if ((c & 0xF0) == 0xE0) {
                ch = c & 0x0F;
                bytes = 3;
            } else if ((c & 0xF8) == 0xF0) {
                ch = c & 0x07;
                bytes = 4;
            } else {
                // Invalid UTF-8
                return std::u32string();
            }
        } else {
            if ((c & 0xC0) != 0x80) {
                // Invalid UTF-8
                return std::u32string();
            }
            ch = (ch << 6) | (c & 0x3F);
            --bytes;
        }
        if (bytes == 1) {
            utf32.push_back(ch);
            ch = 0;
            bytes = 0;
        }
    }
    if (bytes != 0) {
        // Invalid UTF-8
        return std::u32string();
    }
    return utf32;
}

// Function to check if a character is supported by the font
inline bool is_char_supported(FT_Face& face, char32_t character) {
    return FT_Get_Char_Index(face, character) != 0;
}

// Function to check if all characters in a UTF-8 buffer are supported by the font
bool are_all_chars_supported(FT_Face& ftFace, std::string& u8_buffer) {
    std::u32string u32_buffer = utf8_to_utf32(u8_buffer);

    for (char32_t character : u32_buffer) {
        if (!is_char_supported(ftFace, character)) {
            return false;
        }
    }

    return true;
}

bool are_some_chars_supported(FT_Face& ftFace, std::string& u8_buffer) {
    std::u32string u32_buffer = utf8_to_utf32(u8_buffer);

    for (char32_t character : u32_buffer) {
        if (!is_char_supported(ftFace, character)) {
            return false;
        }
    }

    return true;
}

} // namespace utlz