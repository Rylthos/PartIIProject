#pragma once

#include <filesystem>
#include <map>
#include <unordered_map>

#include <glm/glm.hpp>

#include "general.hpp"

namespace ParserImpl {

ParserRet parseObj(std::filesystem::path path, const ParserArgs& args);

ParserRet parseMesh(const std::vector<Triangle>& triangles,
    const std::unordered_map<std::string, Material>& materials,
    const std::map<uint32_t, std::string>& indexToMaterial, const ParserArgs& args);

}
