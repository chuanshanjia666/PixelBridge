#include "filters/Demuxer.h"
#include <iostream>
#include <spdlog/spdlog.h>

namespace pb {

Demuxer::Demuxer(const std::string& url) : Filter("Demuxer"), m_url(url) {}

Demuxer::~Demuxer() {
    stop();
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
    }
}

bool Demuxer::initialize() {
    AVDictionary* options = nullptr;
    
    // Check if it's a UDP stream and apply specific options
    if (m_url.find("udp://") == 0) {
        spdlog::info("Detected UDP stream, applying network options...");
        av_dict_set(&options, "fifo_size", "1000000", 0);
        av_dict_set(&options, "buffer_size", "1000000", 0);
        av_dict_set(&options, "overrun_nonfatal", "1", 0);
        av_dict_set(&options, "timeout", "5000000", 0); // 5 seconds timeout
    }

    if (avformat_open_input(&m_formatCtx, m_url.c_str(), nullptr, &options) != 0) {
        spdlog::error("Could not open input: {}", m_url);
        if (options) av_dict_free(&options);
        return false;
    }

    if (options) av_dict_free(&options);

    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        spdlog::error("Could not find stream information");
        return false;
    }

    for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }

    if (m_videoStreamIndex == -1) {
        spdlog::error("Could not find video stream");
        return false;
    }

    spdlog::info("Demuxer initialized for URL: {}", m_url);
    if (m_formatCtx->iformat) {
        spdlog::info("Input format: {}", m_formatCtx->iformat->long_name);
    }
    return true;
}

void Demuxer::process(DataPacket::Ptr packet) {
    // Demuxer is a source filter, it doesn't process incoming packets.
}

void Demuxer::start() {
    m_running = true;
    m_thread = std::thread(&Demuxer::run, this);
}

AVCodecParameters* Demuxer::getVideoCodecParameters() const {
    if (m_videoStreamIndex == -1) return nullptr;
    return m_formatCtx->streams[m_videoStreamIndex]->codecpar;
}

void Demuxer::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Demuxer::run() {
    while (m_running) {
        auto pktWrapper = std::make_shared<AVPacketWrapper>();
        if (av_read_frame(m_formatCtx, pktWrapper->get()) >= 0) {
            if (pktWrapper->get()->stream_index == m_videoStreamIndex) {
                if (m_next) {
                    m_next->process(pktWrapper);
                }
            }
        } else {
            // EOF or error
            m_running = false;
        }
    }
}

} // namespace pb
