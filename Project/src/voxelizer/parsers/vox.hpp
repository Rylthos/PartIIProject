#pragma once

#include <filesystem>

#include "general.hpp"

namespace ParserImpl {

ParserRet parseVox(std::filesystem::path path, const ParserArgs& args);

}
