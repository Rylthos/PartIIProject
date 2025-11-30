#pragma once

#include <string>

struct ParserArgs {
    std::string filename = "";
    bool flag_all = false;
    bool flag_grid = false;
    bool flag_texture = false;
    bool flag_octree = false;
    bool flag_contree = false;
    bool flag_brickmap = false;
};

class Parser {
  public:
    Parser(ParserArgs args);
};
