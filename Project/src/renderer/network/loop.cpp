#include "loop.hpp"

#include "logger/logger.hpp"
#include "network_proto/header.pb.h"

#include "messages.hpp"

#include "callbacks.hpp"
#include "node.hpp"

#include <msquic.h>
#include <queue>
#include <sys/socket.h>

namespace Network {

std::queue<std::tuple<NetProto::Type, std::vector<uint8_t>, std::vector<uint8_t>>> s_Messages;
std::mutex s_MessageLock;

std::unordered_map<NetProto::Type,
    std::vector<std::function<bool(const std::vector<uint8_t>&, uint32_t)>>>
    s_Callbacks;
std::mutex s_CallbackLock;

void quicStreamMessage(
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

    size_t bufferCount;
    QUIC_BUFFER* quicBuffers;
    std::tie(bufferCount, quicBuffers) = formStreamBuffers(header, data);

    if (QUIC_FAILED(status = s_QuicAPI->StreamSend(
                        stream, quicBuffers, bufferCount, QUIC_SEND_FLAG_NONE, quicBuffers))) {
        goto Error;
    }

    if (QUIC_FAILED(
            status = s_QuicAPI->StreamSend(stream, nullptr, 0, QUIC_SEND_FLAG_FIN, nullptr))) {
        goto Error;
    }

    return;

Error:
    LOG_ERROR("Stream send failed: 0x{:x}", status);
    free(quicBuffers);

    s_QuicAPI->ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    return;
}

void quicDatagramMessage(
    HQUIC connection, const std::vector<uint8_t>& header, const std::vector<uint8_t>& data)
{
    if (connection == nullptr)
        return;

    QUIC_STATUS status;

    std::vector<uint8_t*> buffers = formDatagramBuffers(header, data);

    LOG_DEBUG("[conn][{}] Sending datagram : {}", fmt::ptr(connection), buffers.size());

    for (uint32_t i = 0; i < buffers.size(); i++) {
        if (QUIC_FAILED(status = s_QuicAPI->DatagramSend(connection, (QUIC_BUFFER*)buffers[i], 1,
                            QUIC_SEND_FLAG_NONE, buffers[i]))) {
            goto Error;
        }
    }

    return;

Error:
    LOG_ERROR("Datagram send failed: 0x{:x}", status);

    for (size_t i = 0; i < buffers.size(); i++) {
        free(buffers[i]);
    }

    s_QuicAPI->ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    return;
}

void handleReceive(
    NetProto::Header& header, const std::vector<uint8_t>& data, uint32_t messageID, uint32_t offset)
{
    if (!s_Callbacks.contains(header.type())) {
        LOG_ERROR("No callback set for {}", (uint8_t)header.type());
        return;
    }

    std::vector<uint8_t> trimmed(data.size() - offset);
    memcpy(trimmed.data(), data.data() + offset, data.size() - offset);

    const auto& callbacks = s_Callbacks[header.type()];

    for (const auto& callback : callbacks) {
        if (callback(trimmed, messageID)) {
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

            NetProto::Type type = std::get<0>(msg);

            bool sendStream = true;
            switch (type) {
            case NetProto::HEADER_TYPE_FRAME:
                sendStream = false;
                break;
            default:
                break;
            }

            LOG_DEBUG("[NETWORK] sending: {} | {}", (uint8_t)type, std::get<2>(msg).size());

            if (sendStream) {
                quicStreamMessage(s_Node.connection, std::get<1>(msg), std::get<2>(msg));
            } else {
                quicDatagramMessage(s_Node.connection, std::get<1>(msg), std::get<2>(msg));
            }

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
    header.set_message_size(data.size());

    size_t headerSize = header.ByteSizeLong();

    std::vector<uint8_t> headerBuf(headerSize + 1);
    headerBuf[0] = headerSize;
    header.SerializeToArray(headerBuf.data() + 1, headerSize);

    {
        std::lock_guard<std::mutex> _lock(s_MessageLock);
        s_Messages.push(std::make_tuple(headerType, headerBuf, data));
    }
}

void addCallback(
    NetProto::Type type, std::function<bool(const std::vector<uint8_t>&, uint32_t)> callback)
{
    std::unique_lock _lock(s_CallbackLock);

    s_Callbacks[type].push_back(callback);
}

}
