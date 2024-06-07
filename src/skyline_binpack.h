/** @file skyline_binpack.h
@author Jukka Jylänki

@brief Implements different bin packer algorithms that use the SKYLINE data structure.

 This work is released to Public Domain, do whatever you want with it.
         */
#pragma once

#include <vector>

namespace binpack
{

struct Rect
{
    int x;
    int y;
    int width;
    int height;
};

/** Implements bin packing algorithms that use the SKYLINE data structure to store the bin contents.
 */
class SkylineBinPack
{
public:
    /// Instantiates a bin of size (0,0). Call init to create a new bin.
    SkylineBinPack();

    /// Instantiates a bin of the given size.
    SkylineBinPack(int width, int height);

    /// (Re)initializes the packer to an empty bin of width x height units. Call whenever
    /// you need to restart with a new bin.
    void init(int width, int height);

    /// Inserts a single rectangle into the bin.
    Rect insert(int width, int height);

    /// Computes the ratio of used surface area to the total bin area.
    float occupancy() const;

private:
    int bin_width;
    int bin_height;

    /// Represents a single level (a horizontal line) of the skyline/horizon/envelope.
    struct SkylineNode
    {
        /// The starting x-coordinate (leftmost).
        int x;

        /// The y-coordinate of the skyline level line.
        int y;

        /// The line width. The ending coordinate (inclusive) will be x + width - 1.
        int width;
    };

    std::vector<SkylineNode> skyline;

    unsigned long used_surface_area;

    Rect insert_bottom_left(int width, int height);

    Rect find_position_for_new_node_bottom_left(int width, int height, int& bestHeight, int& bestWidth, int& bestIndex) const;

    bool rectangle_fits(int skylineNodeIndex, int width, int height, int& y) const;

    void add_skyline_level(int skylineNodeIndex, const Rect& rect);

    /// Merges all skyline nodes that are at the same level.
    void merge_skylines();
};

} // namespace binpack
