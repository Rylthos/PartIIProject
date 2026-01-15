#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Network::Serializer {

void writeByte(uint8_t byte, std::vector<uint8_t>& data);
uint8_t readByte(const std::vector<uint8_t>& data, size_t len, size_t& index);

void writeUint32_t(uint32_t bytes, std::vector<uint8_t>& data);
uint32_t readUint32_t(const std::vector<uint8_t>& data, size_t len, size_t& index);

void writeString(std::string str, std::vector<uint8_t>& data);
std::string readString(const std::vector<uint8_t>& data, size_t len, size_t& index);

}
