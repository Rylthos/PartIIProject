#include "loop.hpp"

#include "logger/logger.hpp"
#include "network_proto/header.pb.h"

#include "callbacks.hpp"
#include "node.hpp"

#include <msquic.h>
#include <queue>
#include <sys/socket.h>

namespace Network {

std::queue<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> s_Messages;
std::mutex s_MessageLock;

std::unordered_map<NetProto::Type, std::vector<std::function<bool(const std::vector<uint8_t>&)>>>
    s_Callbacks;
std::mutex s_CallbackLock;

void quicMessage(
    HQUIC connection, const std::vector<uint8_t>& header, const std::vector<uint8_t>& data)
{
    if (connection == nullptr)
        return;

    QUIC_STATUS status;
    HQUIC stream = nullptr;

    LOG_DEBUG("[conn][{}] Sending data", fmt::ptr(connection));

    if (QUIC_FAILED(status = s_QuicAPI->StreamOpen(connection, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                        streamCallback, NULL, &stream))) {
        LOG_ERROR("Stream open failed 0x{:x}", status);
        s_QuicAPI->ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        return;
    }

    LOG_DEBUG("[strm][{}] Starting stream", fmt::ptr(stream));

    if (QUIC_FAILED(status = s_QuicAPI->StreamStart(stream, QUIC_STREAM_START_FLAG_NONE))) {
        LOG_ERROR("Failed to start stream 0x{:x}", status);
        s_QuicAPI->StreamClose(stream);
        s_QuicAPI->ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        return;
    }

    // Transmit header
    uint8_t* rawHeaderBuf = (uint8_t*)malloc(sizeof(QUIC_BUFFER) + header.size());
    QUIC_BUFFER* headerBuf = (QUIC_BUFFER*)rawHeaderBuf;
    headerBuf->Length = header.size();
    headerBuf->Buffer = rawHeaderBuf + sizeof(QUIC_BUFFER);
    memcpy(headerBuf->Buffer, header.data(), header.size());

    const size_t bufferSize = 64 * 1024;
    size_t bufferCount = std::ceil(data.size() / (float)bufferSize);

    uint8_t* rawBuffers = (uint8_t*)malloc(sizeof(QUIC_BUFFER) * bufferCount + data.size());
    QUIC_BUFFER* buffers = (QUIC_BUFFER*)rawBuffers;
    uint8_t* dataStart = rawBuffers + sizeof(QUIC_BUFFER) * bufferCount;
    size_t total = 0;
    for (size_t i = 0; i < bufferCount; i++) {
        size_t length = std::min(bufferSize, data.size() - total);
        buffers[i].Length = length;
        buffers[i].Buffer = dataStart + total;
        memcpy(buffers[i].Buffer, data.data() + total, length);

        total += length;
    }

    if (QUIC_FAILED(status
            = s_QuicAPI->StreamSend(stream, headerBuf, 1, QUIC_SEND_FLAG_START, headerBuf))) {
        LOG_ERROR("Stream send failed: 0x{:x}", status);
        free(headerBuf);
        free(rawBuffers);

        s_QuicAPI->ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        return;
    }

    if (QUIC_FAILED(status
            = s_QuicAPI->StreamSend(stream, buffers, bufferCount, QUIC_SEND_FLAG_NONE, buffers))) {
        LOG_ERROR("Stream send failed: 0x{:x}", status);
        free(headerBuf);
        free(rawBuffers);

        s_QuicAPI->ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        return;
    }

    if (QUIC_FAILED(
            status = s_QuicAPI->StreamSend(stream, nullptr, 0, QUIC_SEND_FLAG_FIN, nullptr))) {
        LOG_ERROR("Stream send failed: 0x{:x}", status);
        free(headerBuf);
        free(rawBuffers);

        s_QuicAPI->ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        return;
    }
}

void handleReceive(NetProto::Header& header, const std::vector<uint8_t>& data, uint32_t offset)
{
    if (!s_Callbacks.contains(header.type()))
        return;

    std::vector<uint8_t> trimmed(data.size() - offset);
    memcpy(trimmed.data(), data.data() + offset, data.size() - offset);

    const auto& callbacks = s_Callbacks[header.type()];

    for (const auto& callback : callbacks) {
        if (callback(trimmed)) {
            break;
        }
    }
}

void writeLoop(std::stop_token stoken)
{
    std::vector<uint8_t> headerBuffer(sizeof(NetProto::Header));

    while (!stoken.stop_requested()) {

        while (!s_Messages.empty()) {
            const auto& msg = s_Messages.front();

            LOG_DEBUG("[NETWORK] sending: {}", msg.second.size());

            quicMessage(s_Node.connection, msg.first, msg.second);

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

    std::vector<uint8_t> headerBuf(headerSize + 1);
    headerBuf[0] = header.ByteSizeLong();
    header.SerializeToArray(headerBuf.data() + 1, headerSize);

    std::vector<uint8_t> buf(data.size());
    memcpy(buf.data(), data.data(), data.size());

    {
        std::lock_guard<std::mutex> _lock(s_MessageLock);
        s_Messages.push(std::make_pair(headerBuf, buf));
    }
}

void addCallback(NetProto::Type type, std::function<bool(const std::vector<uint8_t>&)> callback)
{
    std::unique_lock _lock(s_CallbackLock);

    s_Callbacks[type].push_back(callback);
}

}
