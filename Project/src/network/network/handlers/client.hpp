#pragma once

#include "../setup.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Network::Client {

typedef std::function<void(std::optional<std::vector<uint8_t>>)> StandardCallback;

void run(const Node& node);

void addFileRequest(std::string path, const StandardCallback& callback);

}
