#pragma once
#include <filesystem>

#include <glm/glm.hpp>

#include "general.hpp"

namespace ParserImpl {

ParserRet parseGltf(std::filesystem::path path, const ParserArgs& args);

}
