#pragma once

#include "../setup.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Network::Client {

typedef std::function<void(std::vector<uint8_t>)> StandardCallback;

void run(const Node& node);

void addDirEntryRequest(std::string path, StandardCallback callback);
void addFileEntryRequest(std::string path, StandardCallback callback);

}
