#pragma once

#include <cstdint>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace Encoder {

struct EncoderInfo {
    const AVCodec* codec;
    AVCodecContext* context;
    SwsContext* swsContext;
    AVPacket* packet;
    AVFrame* frame;
};

EncoderInfo setup(uint32_t width, uint32_t height, bool encode);

std::vector<uint8_t> compressVideoFrame(EncoderInfo& info, uint8_t* data, int width, int height);

std::vector<uint8_t> uncompressVideoFrame(
    EncoderInfo& info, const std::vector<uint8_t>& data, int width, int height);

void cleanup(EncoderInfo info);
}
