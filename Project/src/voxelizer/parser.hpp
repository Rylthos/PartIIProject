#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <unordered_map>

#include "modification/diff.hpp"
#include "modification/mod_type.hpp"

#include "parser_args.hpp"
#include "parsers/general.hpp"

enum Structure { GRID = 0, TEXTURE = 1, OCTREE = 2, CONTREE = 3, BRICKMAP = 4, AS_COUNT = 5 };

class Parser {
  public:
    Parser(ParserArgs args);
    ~Parser();

  private:
    ParserImpl::ParserRet parseFile();

    void generateStructures(glm::uvec3 dimensions,
        const std::vector<std::unordered_map<glm::ivec3, glm::vec3>>& frames);

    std::vector<std::unordered_map<glm::ivec3, Modification::DiffType>> generateAnimations(
        const std::vector<std::unordered_map<glm::ivec3, glm::vec3>>& frames,
        glm::uvec3 dimensions);

  private:
    ParserArgs m_Args;

    bool m_ValidStructures[AS_COUNT];
};
