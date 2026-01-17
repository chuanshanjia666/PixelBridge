#include "filters/VideoDecoder.h"
#include <iostream>
#include <spdlog/spdlog.h>

namespace pb {

static enum AVPixelFormat g_hw_pix_fmt = AV_PIX_FMT_NONE;

VideoDecoder::VideoDecoder(AVCodecParameters* params, const std::string& hwTypeName) 
    : Filter("VideoDecoder"), m_codecParams(params) {
    if (!hwTypeName.empty()) {
        m_hwType = av_hwdevice_find_type_by_name(hwTypeName.c_str());
        if (m_hwType == AV_HWDEVICE_TYPE_NONE) {
            spdlog::error("Hardware device type not supported: {}", hwTypeName);
        }
    }
}

VideoDecoder::~VideoDecoder() {
    stop();
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }
    if (m_hwDeviceCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
    }
}

enum AVPixelFormat VideoDecoder::get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
    const enum AVPixelFormat* p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == g_hw_pix_fmt)
            return *p;
    }
    spdlog::error("Failed to get HW surface format.");
    return AV_PIX_FMT_NONE;
}

bool VideoDecoder::init_hw_decoder(AVHWDeviceType type) {
    int err = av_hwdevice_ctx_create(&m_hwDeviceCtx, type, nullptr, nullptr, 0);
    if (err < 0) {
        spdlog::error("Failed to create specified HW device: {}", av_hwdevice_get_type_name(type));
        return false;
    }
    return true;
}

bool VideoDecoder::initialize() {
    m_codec = avcodec_find_decoder(m_codecParams->codec_id);
    if (!m_codec) {
        spdlog::error("Codec not found");
        return false;
    }

    if (m_hwType != AV_HWDEVICE_TYPE_NONE) {
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(m_codec, i);
            if (!config) {
                spdlog::warn("Decoder {} does not support device type {}", 
                          m_codec->name, av_hwdevice_get_type_name(m_hwType));
                m_hwType = AV_HWDEVICE_TYPE_NONE; // Fallback to software
                break;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == m_hwType) {
                g_hw_pix_fmt = config->pix_fmt;
                break;
            }
        }
    }

    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        spdlog::error("Could not allocate video codec context");
        return false;
    }

    if (avcodec_parameters_to_context(m_codecCtx, m_codecParams) < 0) {
        spdlog::error("Failed to copy codec parameters to decoder context");
        return false;
    }

    if (m_hwType != AV_HWDEVICE_TYPE_NONE) {
        if (init_hw_decoder(m_hwType)) {
            m_codecCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
            m_codecCtx->get_format = get_hw_format;
            spdlog::info("Hardware acceleration initialized: {}", av_hwdevice_get_type_name(m_hwType));
        }
    } else {
        spdlog::info("Using software decoding for {}", m_codec->name);
    }

    if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
        spdlog::error("Could not open codec");
        return false;
    }

    return true;
}

void VideoDecoder::process(DataPacket::Ptr packet) {
    if (packet->type() != PacketType::AV_PACKET) return;

    auto pktWrapper = std::static_pointer_cast<AVPacketWrapper>(packet);
    int ret = avcodec_send_packet(m_codecCtx, pktWrapper->get());
    if (ret < 0) {
        spdlog::error("Error sending a packet for decoding");
        return;
    }

    while (ret >= 0) {
        auto frameWrapper = std::make_shared<AVFrameWrapper>();
        ret = avcodec_receive_frame(m_codecCtx, frameWrapper->get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            spdlog::error("Error during decoding");
            return;
        }

        AVFrame* finalFrame = frameWrapper->get();
        DataPacket::Ptr outputPacket = frameWrapper;

        // If it's a HW frame, we might need to transfer it to CPU for simple Sinks
        if (finalFrame->format == g_hw_pix_fmt) {
            auto swFrameWrapper = std::make_shared<AVFrameWrapper>();
            if (av_hwframe_transfer_data(swFrameWrapper->get(), finalFrame, 0) < 0) {
                spdlog::error("Error transferring the data to system memory");
                continue;
            }
            // Copy metadata if needed
            swFrameWrapper->get()->pts = finalFrame->pts;
            swFrameWrapper->get()->pkt_dts = finalFrame->pkt_dts;
            outputPacket = swFrameWrapper;
        }

        if (m_next) {
            m_next->process(outputPacket);
        }
    }
}

void VideoDecoder::stop() {
    // Nothing specific to stop here since it's driven by process()
}

} // namespace pb
