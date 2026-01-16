#pragma once

#include <cstdint>
#include <vector>

namespace Network {

enum class HeaderType : uint8_t {
    REQUEST_FILE_ENTRIES = 0,
    REQUEST_DIR_ENTRIES = 1,
    RETURN,
};

struct Header {
    HeaderType type;
    uint32_t id;
    uint32_t size;
};

Header parseHeader(const std::vector<uint8_t>& data, size_t len, size_t& index);

void addHeader(Header header, std::vector<uint8_t>& data);

}
