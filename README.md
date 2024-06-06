# Render unicode characters

Rendering "all" possible characters with a "simple" setup. A touch of opinionated structure and a playground to explore these dependencies.

Bulk of the work done by: 
https://github.com/zhuyie/drawtext-gl-freetype-harfbuzz.git

# Dependencies

- freetype2
- harfbuzz
- glfw
- glad
- glm

# Running from scratch

    git clone --recurse-submodules ...
    cmake -GNinja -Bbuild .
    cmake --build build

