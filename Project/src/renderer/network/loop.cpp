#include "loop.hpp"

#include "header.hpp"

#include "logger/logger.hpp"

#include <queue>
#include <sys/socket.h>

namespace Network {

std::queue<std::vector<uint8_t>> s_Messages;
std::mutex s_MessageLock;

void readLoop(Node node, std::stop_token stoken)
{
    std::vector<uint8_t> headerBuffer(sizeof(Header));

    int targetFD = node.clientSocket.value_or(node.socket);

    while (!stoken.stop_requested()) {
        ssize_t bits = -1;
        bits = recv(targetFD, headerBuffer.data(), headerBuffer.size(), 0);

        if (bits == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                LOG_ERROR("[NETWORK] Error receving {}", strerror(errno));
            }
            continue;
        }

        if (bits == headerBuffer.size()) {
            uint32_t index = 0;
            Header header = readHeader(headerBuffer.data(), index);

            std::vector<uint8_t> readBuffer(header.size);

            ssize_t read = recv(targetFD, readBuffer.data(), readBuffer.size(), 0);
        } else if (bits > 0) {
            LOG_ERROR("[NETWORK] Unknown number of bits read: {}", bits);
        }
    }
}

void writeLoop(Node node, std::stop_token stoken)
{
    std::vector<uint8_t> headerBuffer(sizeof(Header));

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

void sendMessage(HeaderType headerType, const std::vector<uint8_t>& data)
{
    Header header;
    header.size = data.size();
    header.type = headerType;

    std::vector<uint8_t> message(sizeof(Header));
    message.reserve(sizeof(Header) + data.size());

    uint32_t offset = 0;
    writeHeader(header, message.data(), offset);

    message.insert(message.end(), data.begin(), data.end());

    {
        std::lock_guard<std::mutex> _lock(s_MessageLock);
        s_Messages.push(message);
    }
}

}
