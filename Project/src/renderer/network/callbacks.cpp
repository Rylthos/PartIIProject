#include "callbacks.hpp"

#include "logger/logger.hpp"
#include "network_proto/datagram_header.pb.h"
#include "network_proto/stream_header.pb.h"
#include "node.hpp"
#include <msquic.h>
#include <unordered_map>

#include "network_proto/header.pb.h"

#include "loop.hpp"

struct DatagramMessage {
    uint32_t fragmentCount;
    uint32_t receivedCount;

    std::vector<std::vector<uint8_t>> fragments;
};

struct StreamMessage {
    uint32_t messageLength;
    uint32_t receivedLength;

    std::vector<uint8_t> stream;
};

static std::unordered_map<uint32_t, DatagramMessage> s_DatagramData;
static std::unordered_map<uint32_t, StreamMessage> s_StreamData;
static std::unordered_map<HQUIC, uint32_t> s_StreamToMessage;

namespace Network {

void getHeader(const std::vector<uint8_t>& data, NetProto::Header& header)
{
    uint32_t header_size = data[0];
    header.ParseFromArray(data.data() + 1, header_size);
}

void handleStreamData(uint32_t messageID)
{
    const StreamMessage& message = s_StreamData[messageID];
    if (message.messageLength == 0) {
        LOG_INFO("Received no data");
        s_StreamData.erase(messageID);
        return;
    }

    if (message.receivedLength != message.messageLength) {
        LOG_INFO("Received and expected mismatch : {}/{}", message.receivedLength,
            message.messageLength);
        s_StreamData.erase(messageID);
        return;
    }

    NetProto::Header header;
    getHeader(message.stream, header);

    if (header.message_size() + header.ByteSizeLong() + 1 != message.stream.size()) {
        LOG_ERROR("Data size does not agree with header: {}/{}({}+{}+1)", message.stream.size(),
            header.message_size() + header.ByteSizeLong() + 1, header.message_size(),
            header.ByteSizeLong());

        s_StreamData.erase(messageID);

        return;
    }

    handleReceive(header, message.stream, messageID, header.ByteSizeLong() + 1);

    s_StreamData.erase(messageID);
}

void handleDatagramData(uint32_t messageID)
{
    const DatagramMessage& message = s_DatagramData[messageID];
    size_t total = 0;
    for (const auto& fragment : message.fragments) {
        total += fragment.size();
    }

    if (total == 0) {
        s_DatagramData.erase(messageID);
        return;
    }

    std::vector<uint8_t> data(total);
    size_t offset = 0;
    for (const auto& fragment : message.fragments) {
        memcpy(data.data() + offset, fragment.data(), fragment.size());
        offset += fragment.size();
    }

    NetProto::Header header;
    getHeader(data, header);

    if (header.message_size() + header.ByteSizeLong() + 1 != data.size()) {
        LOG_ERROR("Data size does not agree with header: {}/{}({}+{}+1)", data.size(),
            header.message_size() + header.ByteSizeLong() + 1, header.message_size(),
            header.ByteSizeLong());

        s_DatagramData.erase(messageID);

        return;
    }

    handleReceive(header, data, messageID, header.ByteSizeLong() + 1);

    s_DatagramData.erase(messageID);
}

QUIC_STATUS streamCallback(HQUIC stream, void* context, QUIC_STREAM_EVENT* event)
{
    switch (event->Type) {
    case QUIC_STREAM_EVENT_SEND_COMPLETE:
        free(event->SEND_COMPLETE.ClientContext);
        LOG_DEBUG("[strm][{}] Data Sent", fmt::ptr(stream));
        break;
    case QUIC_STREAM_EVENT_RECEIVE: {
        LOG_DEBUG("[strm][{}] Data Recevied: {} | {}", fmt::ptr(stream),
            event->RECEIVE.TotalBufferLength, event->RECEIVE.BufferCount);

        bool handleHeader = false;
        if (!s_StreamToMessage.contains(stream)) {
            handleHeader = true;
        }

        for (uint32_t i = 0; i < event->RECEIVE.BufferCount; i++) {
            const QUIC_BUFFER buf = event->RECEIVE.Buffers[i];
            if (handleHeader) {
                uint32_t streamHeaderSize = buf.Buffer[0];

                NetProto::StreamHeader streamHeader;
                streamHeader.ParseFromArray(buf.Buffer + 1, streamHeaderSize);

                if (streamHeader.message_length() == 0)
                    break;

                uint32_t messageID = streamHeader.message_id();
                s_StreamToMessage[stream] = messageID;

                s_StreamData[messageID] = {};
                StreamMessage& message = s_StreamData[messageID];
                message.messageLength = streamHeader.message_length();
                message.receivedLength = 0;
                message.stream.resize(message.messageLength);

                memcpy(message.stream.data(), buf.Buffer + streamHeaderSize + 1,
                    buf.Length - streamHeaderSize - 1);

                message.receivedLength += buf.Length - streamHeaderSize - 1;

                handleHeader = false;
            } else {
                uint32_t messageID = s_StreamToMessage[stream];

                StreamMessage& message = s_StreamData[messageID];

                memcpy(message.stream.data() + message.receivedLength, buf.Buffer, buf.Length);

                message.receivedLength += buf.Length;
            }
        }

        break;
    }
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        LOG_DEBUG("[strm][{}] Peer aborted", fmt::ptr(stream));
        s_QuicAPI->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
        break;
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        LOG_DEBUG("[strm][{}] Peer shutdown", fmt::ptr(stream));

        break;
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        LOG_DEBUG("[strm][{}] Done", fmt::ptr(stream));

        if (s_StreamToMessage.contains(stream))
            handleStreamData(s_StreamToMessage[stream]);

        s_StreamToMessage.erase(stream);

        if (!event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
            s_QuicAPI->StreamClose(stream);
        }
        break;
    default:
        break;
    }

    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS listenerCallback(HQUIC listener, void* context, QUIC_LISTENER_EVENT* event)
{
    QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;

    switch (event->Type) {
    case QUIC_LISTENER_EVENT_NEW_CONNECTION: {
        LOG_DEBUG("[list][{}] New connection", fmt::ptr(listener));

        s_Node.connection = event->NEW_CONNECTION.Connection;

        BOOLEAN enableDatagram = TRUE;
        s_QuicAPI->SetParam(s_Node.connection, QUIC_PARAM_CONN_DATAGRAM_SEND_ENABLED,
            sizeof(enableDatagram), &enableDatagram);

        s_QuicAPI->SetCallbackHandler(
            event->NEW_CONNECTION.Connection, (void*)connectionCallback, NULL);

        status = s_QuicAPI->ConnectionSetConfiguration(
            event->NEW_CONNECTION.Connection, s_QuicConfiguration);

        break;
    }
    default:
        break;
    }

    return status;
}

QUIC_STATUS connectionCallback(HQUIC connection, void* context, QUIC_CONNECTION_EVENT* event)
{
    switch (event->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
        LOG_DEBUG("[conn][{}] Connected", fmt::ptr(connection));

        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
        if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
            LOG_DEBUG("[conn][{}] Successfully shut down on idle", fmt::ptr(connection));
        } else {
            LOG_DEBUG("[conn][{}] Shut down by transport 0x{:x}", fmt::ptr(connection),
                event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
        }
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        LOG_DEBUG("[conn][{}] Shut down by peer, {}", fmt::ptr(connection),
            event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        LOG_DEBUG("[conn][{}] All done", fmt::ptr(connection));
        if (!event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
            s_QuicAPI->ConnectionClose(connection);
        }
        break;
    case QUIC_CONNECTION_EVENT_RESUMPTION_TICKET_RECEIVED:
        LOG_DEBUG("[conn][{}] Resumption ticket received ({} bytes)", fmt::ptr(connection),
            event->RESUMPTION_TICKET_RECEIVED.ResumptionTicketLength);
        for (uint32_t i = 0; i < event->RESUMPTION_TICKET_RECEIVED.ResumptionTicketLength; i++) {
            LOG_DEBUG("\t%.2X", (uint8_t)event->RESUMPTION_TICKET_RECEIVED.ResumptionTicket[i]);
        }
        break;
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
        LOG_DEBUG("[strm][{}] Peer started", fmt::ptr(event->PEER_STREAM_STARTED.Stream));

        s_QuicAPI->SetCallbackHandler(
            event->PEER_STREAM_STARTED.Stream, (void*)streamCallback, NULL);
        break;

    case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED: {
        const QUIC_BUFFER* buf = event->DATAGRAM_RECEIVED.Buffer;
        uint8_t headerSize = buf->Buffer[0];

        NetProto::DatagramHeader header;
        header.ParseFromArray(buf->Buffer + 1, headerSize);

        uint32_t messageID = header.message_id();
        uint32_t fragmentIndex = header.fragment_index();
        if (!s_DatagramData.contains(messageID)) {
            s_DatagramData[messageID] = {};
            s_DatagramData[messageID].fragmentCount = header.fragment_count();
            s_DatagramData[messageID].receivedCount = 1;
            s_DatagramData[messageID].fragments.resize(header.fragment_count());

            s_DatagramData[messageID].fragments[fragmentIndex].resize(buf->Length - headerSize - 1);
            memcpy(s_DatagramData[messageID].fragments[fragmentIndex].data(),
                buf->Buffer + headerSize + 1, buf->Length - headerSize - 1);
        } else {
            s_DatagramData[messageID].receivedCount += 1;
            s_DatagramData[messageID].fragments[fragmentIndex].resize(buf->Length - headerSize - 1);
            memcpy(s_DatagramData[messageID].fragments[fragmentIndex].data(),
                buf->Buffer + headerSize + 1, buf->Length - headerSize - 1);
        }

        if (s_DatagramData[messageID].fragmentCount == s_DatagramData[messageID].receivedCount) {
            handleDatagramData(messageID);
        }

        /* cleanup old messages */
        break;
    }
    case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
        if (event->DATAGRAM_SEND_STATE_CHANGED.State == QUIC_DATAGRAM_SEND_SENT) {
            free(event->DATAGRAM_SEND_STATE_CHANGED.ClientContext);
        }

    case QUIC_CONNECTION_EVENT_IDEAL_PROCESSOR_CHANGED:
        LOG_DEBUG("[conn][{}] Ideal Processor is: {}, Partition Index {}", fmt::ptr(connection),
            event->IDEAL_PROCESSOR_CHANGED.IdealProcessor,
            event->IDEAL_PROCESSOR_CHANGED.PartitionIndex);
        break;
    default:
        break;
    }

    return QUIC_STATUS_SUCCESS;
}

}
