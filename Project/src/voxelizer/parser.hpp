#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <glm/glm.hpp>

struct ParserArgs {
    std::string filename = "";
    bool flag_all = false;
    bool flag_grid = false;
    bool flag_texture = false;
    bool flag_octree = false;
    bool flag_contree = false;
    bool flag_brickmap = false;
};

struct Triangle {
    glm::vec3 positions[3];
    glm::vec3 texture[3];
};

class Parser {
  public:
    Parser(ParserArgs args);

    void parseFile();

  private:
    void parseObj(std::filesystem::path filepath);

  private:
    ParserArgs m_Args;

    std::map<glm::ivec3, glm::vec3> m_Voxels;
    std::vector<Triangle> m_Triangles;
};
