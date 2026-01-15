#include "serializers.hpp"

#include <arpa/inet.h>
#include <cassert>

namespace Network::Serializer {

void writeByte(uint8_t byte, std::vector<uint8_t>& data) { data.push_back(byte); }

uint8_t readByte(const std::vector<uint8_t>& data, size_t len, size_t& index)
{
    assert(index < len);

    uint8_t byte = data[index];
    index++;
    return byte;
}

void writeUint32_t(uint32_t bytes, std::vector<uint8_t>& data)
{
    uint32_t converted = htonl(bytes);
    for (uint8_t i = 0; i < 4; i++) {
        writeByte((converted >> (24 - 8 * i)) & 0xFF, data);
    }
}

uint32_t readUint32_t(const std::vector<uint8_t>& data, size_t len, size_t& index)
{
    uint32_t read = 0;
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t byte = readByte(data, len, index);

        read <<= 8;
        read |= byte;
    }

    return ntohl(read);
}

void writeString(std::string str, std::vector<uint8_t>& data)
{
    writeUint32_t(str.length(), data);
    for (size_t i = 0; i < str.length(); i++) {
        data.push_back(str.at(i));
    }
}

std::string readString(const std::vector<uint8_t>& data, size_t len, size_t& index)
{
    uint32_t length = readUint32_t(data, len, index);

    std::string str;

    str.resize(length);

    for (uint32_t i = 0; i < length; i++) {
        str.at(i) = (char)readByte(data, len, index);
    }

    return str;
}

}
