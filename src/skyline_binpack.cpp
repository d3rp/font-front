#include "skyline_binpack.h"
#include <algorithm>
#include <limits>
#include <cassert>

namespace binpack
{

SkylineBinPack::SkylineBinPack()
    : bin_width(0)
    , bin_height(0)
    , used_surface_area(0)
{
}

SkylineBinPack::SkylineBinPack(int width, int height)
    : bin_width(0)
    , bin_height(0)
    , used_surface_area(0)
{
    init(width, height);
}

void SkylineBinPack::init(int width, int height)
{
    assert(width > 0);
    assert(height > 0);

    bin_width  = width;
    bin_height = height;

    used_surface_area = 0;
    skyline.clear();
    SkylineNode node;
    node.x     = 0;
    node.y     = 0;
    node.width = bin_width;
    skyline.push_back(node);
}

Rect SkylineBinPack::insert(int width, int height) { return insert_bottom_left(width, height); }

float SkylineBinPack::occupancy() const
{
    return (float) used_surface_area / (bin_width * bin_height);
}

Rect SkylineBinPack::insert_bottom_left(int width, int height)
{
    int best_height;
    int best_width;
    int best_index;
    Rect new_node = find_position_for_new_node_bottom_left(width, height, best_height, best_width, best_index);

    if (best_index != -1)
    {
        add_skyline_level(best_index, new_node);
        used_surface_area += width * height;
    }

    return new_node;
}

Rect SkylineBinPack::find_position_for_new_node_bottom_left(
    int width,
    int height,
    int& best_height,
    int& best_width,
    int& best_index
) const
{
    Rect new_node;
    memset(&new_node, 0, sizeof(new_node));
    best_index  = -1;
    best_height = std::numeric_limits<int>::max();
    best_width  = std::numeric_limits<int>::max();

    for (size_t i = 0; i < skyline.size(); ++i)
    {
        int y;
        if (rectangle_fits(i, width, height, y))
        {
            if (y + height < best_height || (y + height == best_height && skyline[i].width < best_width))
            {
                best_height     = y + height;
                best_index      = i;
                best_width      = skyline[i].width;
                new_node.x      = skyline[i].x;
                new_node.y      = y;
                new_node.width  = width;
                new_node.height = height;
            }
        }
    }

    return new_node;
}

bool SkylineBinPack::rectangle_fits(int skyline_node_index, int width, int height, int& y) const
{
    int x = skyline[skyline_node_index].x;
    if (x + width > bin_width)
        return false;

    int width_left = width;
    int i          = skyline_node_index;
    y              = skyline[skyline_node_index].y;

    while (width_left > 0)
    {
        y = std::max(y, skyline[i].y);
        if (y + height > bin_height)
            return false;
        width_left -= skyline[i].width;
        ++i;
        assert(i < (int) skyline.size() || width_left <= 0);
    }

    return true;
}

void SkylineBinPack::add_skyline_level(int skyline_node_index, const Rect& rect)
{
    SkylineNode new_node;
    new_node.x     = rect.x;
    new_node.y     = rect.y + rect.height;
    new_node.width = rect.width;
    skyline.insert(skyline.begin() + skyline_node_index, new_node);

    assert(new_node.x + new_node.width <= bin_width);
    assert(new_node.y <= bin_height);

    for (size_t i = skyline_node_index + 1; i < skyline.size(); ++i)
    {
        assert(skyline[i - 1].x <= skyline[i].x);

        if (skyline[i].x < skyline[i - 1].x + skyline[i - 1].width)
        {
            int shrink = skyline[i - 1].x + skyline[i - 1].width - skyline[i].x;
            skyline[i].x += shrink;
            skyline[i].width -= shrink;

            if (skyline[i].width <= 0)
            {
                skyline.erase(skyline.begin() + i);
                --i;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    merge_skylines();
}

void SkylineBinPack::merge_skylines()
{
    for (size_t i = 0; i < skyline.size() - 1; ++i)
    {
        if (skyline[i].y == skyline[i + 1].y)
        {
            skyline[i].width += skyline[i + 1].width;
            skyline.erase(skyline.begin() + (i + 1));
            --i;
        }
    }
}

} // End of namespace BinPack