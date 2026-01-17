#ifndef VIDEOSINK_H
#define VIDEOSINK_H

#include "core/Filter.h"
#include <iostream>
#include <spdlog/spdlog.h>

namespace pb {

class VideoSink : public Filter {
public:
    VideoSink() : Filter("VideoSink") {}

    bool initialize() override {
        m_frameCount = 0;
        return true;
    }

    void process(DataPacket::Ptr packet) override {
        if (packet->type() == PacketType::AV_FRAME) {
            auto frameWrapper = std::static_pointer_cast<AVFrameWrapper>(packet);
            AVFrame* frame = frameWrapper->get();
            m_frameCount++;
            if (m_frameCount % 30 == 0) {
                spdlog::debug("[VideoSink] Received {} frames. Resolution: {}x{} Format: {}",
                           m_frameCount, frame->width, frame->height, frame->format);
            }
        }
    }

    void stop() override {
        spdlog::info("[VideoSink] Total frames processed: {}", m_frameCount);
    }

private:
    int m_frameCount = 0;
};

} // namespace pb

#endif // VIDEOSINK_H
