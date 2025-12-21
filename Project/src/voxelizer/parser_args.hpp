#pragma once

#include <cstdint>
#include <string>

struct ParserArgs {
    std::string filename = "";
    std::string output = "";
    std::string name = "";
    bool flag_all = false;
    bool flag_grid = false;
    bool flag_texture = false;
    bool flag_octree = false;
    bool flag_contree = false;
    bool flag_brickmap = false;
    uint32_t voxels_per_unit = 1;
    float units = 128.f;
};
