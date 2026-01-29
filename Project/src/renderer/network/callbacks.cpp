#include "callbacks.hpp"

#include "logger/logger.hpp"
#include "node.hpp"
#include <msquic.h>

#include "network_proto/header.pb.h"

#include "loop.hpp"

namespace Network {

static std::unordered_map<HQUIC, std::vector<uint8_t>> s_ReceivedData;

void handleData(const std::vector<uint8_t>& data)
{
    if (data.size() == 0)
        return;

    NetProto::Header header;
    uint8_t header_size = data[0];

    header.ParseFromArray(data.data() + 1, header_size);

    if (header.header_size() + header_size + 1 != data.size()) {
        LOG_ERROR("Data size does not agree with header");
        return;
    }

    handleReceive(header, data, header.ByteSizeLong() + 1);
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

        std::vector<uint8_t> newBuf(event->RECEIVE.TotalBufferLength);
        size_t offset = 0;
        for (uint32_t i = 0; i < event->RECEIVE.BufferCount; i++) {
            memcpy(newBuf.data() + offset, event->RECEIVE.Buffers[i].Buffer,
                event->RECEIVE.Buffers[i].Length);
            offset += event->RECEIVE.Buffers[i].Length;
        }

        s_ReceivedData[stream].insert(s_ReceivedData[stream].end(), newBuf.begin(), newBuf.end());

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

        handleData(s_ReceivedData[stream]);

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
    case QUIC_LISTENER_EVENT_NEW_CONNECTION:
        LOG_DEBUG("[list][{}] New connection", fmt::ptr(listener));

        s_Node.connection = event->NEW_CONNECTION.Connection;

        s_QuicAPI->SetCallbackHandler(
            event->NEW_CONNECTION.Connection, (void*)connectionCallback, NULL);

        status = s_QuicAPI->ConnectionSetConfiguration(
            event->NEW_CONNECTION.Connection, s_QuicConfiguration);

        break;
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
        // Send?
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

        s_ReceivedData[event->PEER_STREAM_STARTED.Stream].clear();

        s_QuicAPI->SetCallbackHandler(
            event->PEER_STREAM_STARTED.Stream, (void*)streamCallback, NULL);
        break;

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
