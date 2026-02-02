#pragma once

#include <cstdint>
#include <vector>

namespace Compression {
std::vector<uint8_t> compress(const std::vector<uint8_t>& data);

std::vector<uint8_t> uncompress(const std::vector<uint8_t>& data);
}
