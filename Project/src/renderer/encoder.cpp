#include "encoder.hpp"

#include <cassert>
#include <filesystem>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <logger/logger.hpp>

namespace Encoder {

EncoderInfo setup(uint32_t width, uint32_t height, bool encode)
{
    width = (width + 1) & ~1;
    height = (height + 1) & ~1;

    EncoderInfo info;
    if (encode) {
        info.codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    } else {
        info.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    }

    if (!info.codec) {
        LOG_CRITICAL("[COMPRESSION] Encoder not found");
        exit(-1);
    }

    info.context = avcodec_alloc_context3(info.codec);
    if (!info.context) {
        LOG_CRITICAL("[COMPRESSION] Failed to allocated context");
        exit(-1);
    }

    info.context->time_base = { 1, 30 };
    info.context->framerate = { 30, 1 };

    info.context->pix_fmt = AV_PIX_FMT_YUV420P;

    info.context->width = width;
    info.context->height = height;

    info.context->gop_size = 10;
    info.context->max_b_frames = 0;
    info.context->refs = 3;

    info.context->bit_rate = 6000000;

    av_opt_set(info.context->priv_data, "preset", "ultrafast", 0);
    av_opt_set(info.context->priv_data, "crf", "30", 0);
    av_opt_set(info.context->priv_data, "tune", "zerolatency", 0);

    if (avcodec_open2(info.context, info.codec, nullptr) < 0) {
        LOG_CRITICAL("Could not open codec");
        exit(-1);
    }

    if (encode) {
        info.swsContext = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height,
            AV_PIX_FMT_YUV420P, 0, nullptr, nullptr, nullptr);
    } else {
        info.swsContext = sws_getContext(width, height, AV_PIX_FMT_YUV420P, width, height,
            AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    }
    if (!info.swsContext) {
        LOG_CRITICAL("Could not allocate sws context");
        exit(-1);
    }

    info.packet = av_packet_alloc();
    if (!info.packet) {
        LOG_CRITICAL("Failed to allocate packet");
        exit(-1);
    }

    info.frame = av_frame_alloc();
    if (!info.frame) {
        LOG_CRITICAL("Failed to allocate video frame");
        exit(-1);
    }

    info.frame->format = info.context->pix_fmt;
    info.frame->width = info.context->width;
    info.frame->height = info.context->height;

    if (av_frame_get_buffer(info.frame, 0) < 0) {
        LOG_CRITICAL("Failed to allocate video frame data");
        exit(-1);
    }

    return info;
}

std::vector<uint8_t> compressVideoFrame(EncoderInfo& info, uint8_t* data, int width, int height)
{
    if (av_frame_make_writable(info.frame) < 0) {
        LOG_ERROR("Frame data not writable");
        return {};
    }

    const uint8_t* inData[1] = { data };
    int inLineSize[1] = { (int)sizeof(uint32_t) * width };

    sws_scale(
        info.swsContext, inData, inLineSize, 0, height, info.frame->data, info.frame->linesize);

    int ret = avcodec_send_frame(info.context, info.frame);
    if (ret < 0) {
        LOG_ERROR("[COMPRESSION] Failed to send frame: {}", ret);
        return {};
    }

    ret = avcodec_receive_packet(info.context, info.packet);

    std::vector<uint8_t> returnData(info.packet->size);
    memcpy(returnData.data(), info.packet->data, info.packet->size);
    av_packet_unref(info.packet);

    return returnData;
}

std::vector<uint8_t> uncompressVideoFrame(
    EncoderInfo& info, const std::vector<uint8_t>& data, int width, int height)
{
    av_packet_unref(info.packet);

    info.packet->data = (uint8_t*)data.data();
    info.packet->size = data.size();

    int ret = avcodec_send_packet(info.context, info.packet);
    if (ret < 0) {
        LOG_ERROR("[COMPRESSION] Failed to send packet: {}", ret);
        return {};
    }

    std::vector<uint8_t> returnData;
    while (1) {
        ret = avcodec_receive_frame(info.context, info.frame);
        if (ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret < 0) {
            LOG_ERROR("[COMPRESSION] Failed to receive frame");
            switch (ret) {
            case AVERROR(EAGAIN):
                LOG_INFO("EAGAIN");
                break;
            case AVERROR_EOF:
                LOG_INFO("EOF");
                break;
            case AVERROR(EINVAL):
                LOG_INFO("EINVAL");
                break;
            }

            return {};
        }
        returnData.resize(width * height * 4);

        uint8_t* outData[1] = { returnData.data() };
        int outLineSize[1] = { (int)sizeof(uint32_t) * width };
        sws_scale(info.swsContext, info.frame->data, info.frame->linesize, 0, info.frame->height,
            outData, outLineSize);

        av_frame_unref(info.frame);
    }

    return returnData;
}

void cleanup(EncoderInfo info)
{
    av_packet_free(&info.packet);
    av_frame_free(&info.frame);
    avcodec_free_context(&info.context);
    sws_freeContext(info.swsContext);
}
}
