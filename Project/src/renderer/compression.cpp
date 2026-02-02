#include "compression.hpp"

#include <cassert>
#include <filesystem>
#include <zlib.h>

#include <logger/logger.hpp>

namespace Compression {
std::vector<uint8_t> compress(const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> returnData;

    z_stream stream;

    const size_t BUF_SIZE = 128 * 1024;
    std::vector<uint8_t> tempBuffer(BUF_SIZE);

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.next_in = (Bytef*)data.data();
    stream.avail_in = data.size();
    stream.next_out = tempBuffer.data();
    stream.avail_out = tempBuffer.size();
    stream.opaque = Z_NULL;

    int res = deflateInit(&stream, Z_BEST_COMPRESSION);

    while (stream.avail_in != 0) {
        res = deflate(&stream, Z_NO_FLUSH);
        assert(res == Z_OK);

        if (stream.avail_out == 0) {
            returnData.insert(returnData.end(), tempBuffer.begin(), tempBuffer.end());
            stream.next_out = tempBuffer.data();
            stream.avail_out = tempBuffer.size();
        }
    }

    int deflate_res = Z_OK;
    while (deflate_res == Z_OK) {
        if (stream.avail_out == 0) {
            returnData.insert(returnData.end(), tempBuffer.begin(), tempBuffer.end());
            stream.next_out = tempBuffer.data();
            stream.avail_out = tempBuffer.size();
        }
        deflate_res = deflate(&stream, Z_FINISH);
    }

    assert(deflate_res == Z_STREAM_END);
    returnData.insert(
        returnData.end(), tempBuffer.begin(), tempBuffer.begin() + BUF_SIZE - stream.avail_out);
    deflateEnd(&stream);

    return returnData;
}

std::vector<uint8_t> uncompress(const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> returnData;

    z_stream stream;

    const size_t BUF_SIZE = 128 * 1024;
    std::vector<uint8_t> tempBuffer(BUF_SIZE);

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.next_in = (Bytef*)data.data();
    stream.avail_in = data.size();
    stream.next_out = tempBuffer.data();
    stream.avail_out = tempBuffer.size();
    stream.opaque = Z_NULL;

    int res = inflateInit(&stream);

    while (stream.avail_in != 0) {
        res = inflate(&stream, Z_NO_FLUSH);
        if (res == Z_STREAM_ERROR) {
            LOG_ERROR("Inflate failed {}", res);
            assert(false);
        }

        if (stream.avail_out == 0) {
            returnData.insert(returnData.end(), tempBuffer.begin(), tempBuffer.end());
            stream.next_out = tempBuffer.data();
            stream.avail_out = tempBuffer.size();
        }
    }

    int deflate_res = Z_OK;
    while (deflate_res == Z_OK) {
        if (stream.avail_out == 0) {
            returnData.insert(returnData.end(), tempBuffer.begin(), tempBuffer.end());
            stream.next_out = tempBuffer.data();
            stream.avail_out = tempBuffer.size();
        }
        deflate_res = inflate(&stream, Z_FINISH);
    }

    assert(deflate_res == Z_STREAM_END);
    returnData.insert(
        returnData.end(), tempBuffer.begin(), tempBuffer.begin() + BUF_SIZE - stream.avail_out);
    inflateEnd(&stream);

    return returnData;
}
}
