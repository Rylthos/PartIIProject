#pragma once

#include <cstdint>
#include <vector>

namespace Network {

enum class HeaderType : uint8_t {
    REQUEST_FILES = 0,
};

struct Header {
    HeaderType type;
    uint32_t id;
};

Header parseHeader(const std::vector<uint8_t>& data, size_t len, size_t& index);

void addHeader(Header header, std::vector<uint8_t>& data);

}
