#pragma once

#include <cstdint>

namespace Network {

enum class HeaderType : uint8_t {
    FRAME = 0,
};

struct Header {
    uint32_t size;
    HeaderType type;
};

Header readHeader(uint8_t* data, uint32_t& index);

void writeHeader(Header header, uint8_t* data, uint32_t& offset);

}
