#pragma once

#include "msquic.h"

#include <tuple>
#include <vector>

namespace Network {
std::pair<size_t, QUIC_BUFFER*> formStreamBuffers(
    const std::vector<uint8_t>& headerData, const std::vector<uint8_t>& messageData);

std::vector<uint8_t*> formDatagramBuffers(
    const std::vector<uint8_t>& headerData, const std::vector<uint8_t>& messageData);
};
