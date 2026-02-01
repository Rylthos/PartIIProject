#include "messages.hpp"

#include "network_proto/stream_header.pb.h"

#include "logger/logger.hpp"

namespace Network {
uint32_t getNextID()
{
    static uint32_t ID = 0;
    uint32_t v = ID;
    ID += 1;
    return v;
}

std::pair<size_t, QUIC_BUFFER*> formStreamBuffers(
    const std::vector<uint8_t>& headerData, const std::vector<uint8_t>& messageData)
{
    const size_t bufferSize = 32 * 1024;
    const size_t bufferCount = std::ceil(messageData.size() / (float)bufferSize) + 1;

    NetProto::StreamHeader streamHeader;
    streamHeader.set_message_id(getNextID());
    streamHeader.set_message_length(headerData.size() + messageData.size());

    const size_t streamHeaderSize = streamHeader.ByteSizeLong();

    uint8_t* rawBuffer = (uint8_t*)malloc((streamHeaderSize + 1) + sizeof(QUIC_BUFFER) * bufferCount
        + headerData.size() + messageData.size());

    QUIC_BUFFER* quicHeaders = (QUIC_BUFFER*)rawBuffer;
    uint8_t* dataBuf = rawBuffer + sizeof(QUIC_BUFFER) * bufferCount;

    size_t dataOffset = 0;

    QUIC_BUFFER& header = quicHeaders[0];
    header.Length = headerData.size() + streamHeaderSize + 1;
    header.Buffer = dataBuf + dataOffset;
    header.Buffer[0] = streamHeaderSize;

    streamHeader.SerializeToArray(header.Buffer + 1, streamHeaderSize);
    memcpy(header.Buffer + streamHeaderSize + 1, headerData.data(), headerData.size());

    dataOffset += streamHeaderSize + headerData.size() + 1;

    uint32_t total = 0;
    for (uint32_t i = 1; i < bufferCount; i++) {
        size_t bufLength = std::min(bufferSize, messageData.size() - total);

        QUIC_BUFFER& buf = quicHeaders[i];

        buf.Length = bufLength;
        buf.Buffer = dataBuf + dataOffset;

        memcpy(buf.Buffer, messageData.data() + total, bufLength);

        dataOffset += bufLength;
        total += bufLength;
    }

    return std::make_pair(bufferCount, quicHeaders);
}

}
