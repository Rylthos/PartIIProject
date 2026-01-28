#pragma once

#include "network_proto/header.pb.h"

#include "node.hpp"

#include <functional>
#include <stop_token>
#include <vector>

namespace Network {

void readLoop(Node node, std::stop_token stoken);

void writeLoop(Node node, std::stop_token stoken);

void sendMessage(NetProto::Type headerType, const std::vector<uint8_t>& data);

void addCallback(
    NetProto::Type headerType, std::function<bool(const std::vector<uint8_t>&)> callback);

};
