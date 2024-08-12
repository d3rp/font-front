#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H // Include FreeType header files

#include "blueprints.h"

namespace typesetting
{
// Memory handling and other resource mgmnt for fonts
struct Library
{
    bool init()
    {
        if (library)
            return true;

        if (FT_Init_FreeType(&library))
            return false;

        return true;
    }

    bool destroy()
    {
        FT_Done_FreeType(library);
        library = nullptr;

        return true;
    }

    FT_Library get() const { return library; }
    FT_Library library { nullptr };
};
} // namespace typesetting