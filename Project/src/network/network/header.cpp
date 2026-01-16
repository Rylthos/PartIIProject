#include "header.hpp"
#include "network/serializers.hpp"

namespace Network {

Header parseHeader(const std::vector<uint8_t>& data, size_t len, size_t& index)
{
    Header header;
    header.type = static_cast<HeaderType>(Serializer::readByte(data, len, index));
    header.id = Serializer::readUint32_t(data, len, index);
    header.size = Serializer::readUint32_t(data, len, index);
    return header;
}

void addHeader(Header header, std::vector<uint8_t>& data)
{
    Serializer::writeByte(static_cast<uint8_t>(header.type), data);
    Serializer::writeUint32_t(header.id, data);
    Serializer::writeUint32_t(header.size, data);
}

}
