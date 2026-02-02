#pragma once

#include "network_proto/header.pb.h"

#include "node.hpp"

#include <functional>
#include <google/protobuf/message.h>
#include <stop_token>
#include <vector>

namespace Network {

void handleReceive(NetProto::Header& header, const std::vector<uint8_t>& data, uint32_t messageID,
    uint32_t offset = 0);

void writeLoop(std::stop_token stoken);

void sendMessage(NetProto::Type headerType, const google::protobuf::Message& message);

void sendMessage(NetProto::Type headerType, const std::vector<uint8_t>& data);

void addCallback(
    NetProto::Type headerType, std::function<bool(const std::vector<uint8_t>&, uint32_t)> callback);

};
