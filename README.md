# Render unicode characters

Rendering "all" possible characters with a "simple" setup. A touch of opinionated structure and a playground to explore these dependencies.

Cross-platform (Windows, MacOS)

Splits strings into script and font specific runs applying them appropriately.

Most of the rendering pipeline and some of the shaping logic imitated from: 
https://github.com/zhuyie/drawtext-gl-freetype-harfbuzz.git

# License

Note that the submodules and fonts use their own licenses. Most fonts are OFL or common creative, but do check them. This repo uses AGPL which is the most restrictive I found. I've intended this for learning purposes, not copying as is to closed source commercial solutions.

# Dependencies

- freetype2
- harfbuzz
- utfcpp
- opengl
- glfw
- glad
- glm

# Running from scratch

    git clone --recurse-submodules https://github.com/d3rp/font-front.git
    cmake -GNinja -Bbuild .
    cmake --build build

