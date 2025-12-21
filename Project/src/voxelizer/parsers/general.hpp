#pragma once

#include "glm/gtx/hash.hpp"
#include <glm/glm.hpp>

#include <unordered_map>

#include "../parser_args.hpp"

#include <string>

namespace ParserImpl {

typedef std::tuple<glm::uvec3, std::unordered_map<glm::ivec3, glm::vec3>> ParserRet;

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
    bool validTexture = false;

    glm::vec3 diffuse;
};

std::vector<std::string> split(std::string str, std::string delim);
}
