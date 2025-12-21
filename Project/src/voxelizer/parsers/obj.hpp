#pragma once

#include <filesystem>

#include <glm/glm.hpp>

#include "general.hpp"

namespace ParserImpl {

ParserRet parseObj(std::filesystem::path path, const ParserArgs& args);

}
