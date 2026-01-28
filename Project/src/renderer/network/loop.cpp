#include "loop.hpp"

#include "logger/logger.hpp"
#include "network_proto/header.pb.h"

#include <queue>
#include <sys/socket.h>

namespace Network {

std::queue<std::vector<uint8_t>> s_Messages;
std::mutex s_MessageLock;

std::unordered_map<NetProto::Type, std::vector<std::function<bool(const std::vector<uint8_t>&)>>>
    s_Callbacks;
std::mutex s_CallbackLock;

void handleReceive(NetProto::Header& header, const std::vector<uint8_t>& data)
{
    if (!s_Callbacks.contains(header.type()))
        return;

    const auto& callbacks = s_Callbacks[header.type()];

    for (const auto& callback : callbacks) {
        if (callback(data)) {
            break;
        }
    }
}

void readLoop(Node node, std::stop_token stoken)
{
    std::vector<uint8_t> headerSizeBuffer(sizeof(uint8_t));

    int targetFD = node.clientSocket.value_or(node.socket);

    while (!stoken.stop_requested()) {
        ssize_t headerSize = recv(targetFD, headerSizeBuffer.data(), headerSizeBuffer.size(), 0);

        if (headerSize == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                LOG_ERROR("[NETWORK] Error receving {}", strerror(errno));
            }
            continue;
        }

        std::vector<uint8_t> headerBuffer(headerSizeBuffer[0]);
        size_t headerBits = recv(targetFD, headerBuffer.data(), headerBuffer.size(), 0);

        if (headerBits == headerBuffer.size()) {
            NetProto::Header header;

            header.ParseFromArray(headerBuffer.data(), headerBits);

            std::vector<uint8_t> readBuffer(header.header_size());
            size_t readCount = 0;

            while (readCount != header.header_size()) {
                ssize_t read = recv(
                    targetFD, readBuffer.data() + readCount, readBuffer.size() - readCount, 0);

                readCount += read;
            }

            handleReceive(header, readBuffer);

        } else if (headerBits > 0) {
            LOG_ERROR("[NETWORK] Unknown number of bits read: {}", headerBits);
        }
    }
}

void writeLoop(Node node, std::stop_token stoken)
{
    std::vector<uint8_t> headerBuffer(sizeof(NetProto::Header));

    while (!stoken.stop_requested()) {

        while (!s_Messages.empty()) {
            const std::vector<uint8_t>& msg = s_Messages.front();
            if (node.clientSocket.has_value()) {
                send(node.clientSocket.value(), msg.data(), msg.size(), 0);
            } else {
                send(node.socket, msg.data(), msg.size(), 0);
            }

            LOG_DEBUG("[NETWORK] sending: {}", msg.size());

            {
                std::lock_guard<std::mutex> _lock(s_MessageLock);
                s_Messages.pop();
            }
        }
    }
}

void sendMessage(NetProto::Type headerType, const google::protobuf::Message& message)
{
    std::vector<uint8_t> data(message.ByteSizeLong());
    message.SerializeToArray(data.data(), data.size());

    sendMessage(headerType, data);
}

void sendMessage(NetProto::Type headerType, const std::vector<uint8_t>& data)
{
    NetProto::Header header;
    header.set_type(headerType);
    header.set_header_size(data.size());

    size_t headerSize = header.ByteSizeLong();

    std::vector<uint8_t> message(headerSize + 1);

    message[0] = (uint8_t)headerSize;
    header.SerializeToArray(message.data() + 1, headerSize);

    message.insert(message.end(), data.begin(), data.end());

    {
        std::lock_guard<std::mutex> _lock(s_MessageLock);
        s_Messages.push(message);
    }
}

void addCallback(NetProto::Type type, std::function<bool(const std::vector<uint8_t>&)> callback)
{
    std::unique_lock _lock(s_CallbackLock);

    s_Callbacks[type].push_back(callback);
}

}
