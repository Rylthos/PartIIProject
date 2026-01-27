#include "header.hpp"

namespace Network {

Header readHeader(uint8_t* data, uint32_t& index)
{
    Header header;

    header.size = 0;
    for (uint8_t i = 0; i < 4; i++) {
        header.size <<= 8;
        header.size |= (data[index++]);
    }

    uint8_t type = data[index++];
    header.type = static_cast<HeaderType>(type);

    return header;
}

void writeHeader(Header header, uint8_t* data, uint32_t& offset)
{
    data[offset++] = (header.size >> 24) & 0xFF;
    data[offset++] = (header.size >> 16) & 0xFF;
    data[offset++] = (header.size >> 8) & 0xFF;
    data[offset++] = (header.size >> 0) & 0xFF;
    data[offset++] = static_cast<uint8_t>(header.type);
}

}
