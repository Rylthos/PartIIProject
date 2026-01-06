#pragma once

#include "glm/gtx/hash.hpp"
#include <glm/glm.hpp>

#include <unordered_map>

#include <filesystem>

#include "../parser_args.hpp"

#include <string>

namespace ParserImpl {

// Dimensions, Frames(index->colour)
typedef std::tuple<glm::uvec3, std::vector<std::unordered_map<glm::ivec3, glm::vec3>>> ParserRet;

struct Vertex {
    glm::vec3 position;
    glm::vec3 texture;

    std::vector<size_t> boneIDs;
    std::vector<float> boneWeights;
};

struct Triangle {
    Vertex vertices[3];
    int32_t matIndex;
};

struct Material {
    int width;
    int height;
    int colourDepth;
    uint8_t* data;
    bool validTexture = false;

    glm::vec3 diffuse;
};

Triangle transformTriangle(Triangle t, glm::mat4 transform);
Triangle transformTriangle(Triangle t, std::vector<glm::mat4> boneTransforms);

std::vector<std::string> split(std::string str, std::string delim);

ParserRet parseMeshes(const std::vector<std::vector<Triangle>>& meshes,
    const std::unordered_map<int32_t, Material>& materials, const ParserArgs& args);

ParserRet parseMesh(const std::vector<Triangle>& triangles,
    const std::unordered_map<int32_t, Material>& materials, const ParserArgs& args);

void parseImage(std::filesystem::path filepath, Material& material);

}
