#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <unordered_map>

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

enum Structure { GRID = 0, TEXTURE = 1, OCTREE = 2, CONTREE = 3, BRICKMAP = 4, AS_COUNT = 5 };

struct Triangle {
    glm::vec3 positions[3];
    glm::vec3 texture[3];
    int32_t matIndex;
};

struct Material {
    int width;
    int height;
    int colourDepth;
    uint8_t* data;

    glm::vec3 diffuse;
};

class Parser {
  public:
    Parser(ParserArgs args);
    ~Parser();

    void parseFile();

  private:
    void parseObj(std::filesystem::path filepath);

    void parseMesh();

    void parseMaterialLib(std::filesystem::path filepath);

    void generateStructures();

    bool aabbTriangleIntersection(const Triangle& triangle, glm::vec3 cell, glm::vec3 cellSize);

    glm::vec3 calculateTexCoords(const Triangle& triangle, glm::vec3 cell, glm::vec3 cellSize);

    void parseImage(std::filesystem::path filepath, Material& material);

  private:
    ParserArgs m_Args;

    bool m_ValidStructures[AS_COUNT];

    glm::uvec3 m_Dimensions;

    std::map<uint32_t, std::string> m_IndexToMaterial;
    std::map<std::string, uint32_t> m_MaterialToIndex;

    std::unordered_map<std::string, Material> m_Materials;

    std::unordered_map<glm::ivec3, glm::vec3> m_Voxels;
    std::vector<Triangle> m_Triangles;
};
