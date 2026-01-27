#pragma once

#include "header.hpp"
#include "node.hpp"

#include <stop_token>
#include <vector>

namespace Network {

void readLoop(Node node, std::stop_token stoken);

void writeLoop(Node node, std::stop_token stoken);

void sendMessage(HeaderType headerType, const std::vector<uint8_t>& data);

};
