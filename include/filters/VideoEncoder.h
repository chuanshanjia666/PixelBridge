#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include "core/Filter.h"

extern "C"
{
#include <libswscale/swscale.h>
}

namespace pb
{

    class VideoEncoder : public Filter
    {
    public:
        VideoEncoder(const std::string &codecName = "libx264", const std::string &hwType = "");
        ~VideoEncoder();

        bool initialize(int width, int height, int fps = 30);
        bool initialize() override { return false; }

        void process(DataPacket::Ptr packet) override;
        void stop() override;

        AVCodecContext *getCodecContext() const { return m_codecCtx; }

    private:
        bool init_hw_encoder();

        std::string m_codecName;
        std::string m_hwTypeName;
        const AVCodec *m_codec = nullptr;
        AVCodecContext *m_codecCtx = nullptr;
        AVBufferRef *m_hwDeviceCtx = nullptr;
        AVBufferRef *m_hwFramesCtx = nullptr;
        int64_t m_pts = 0;

        // SwsContext for internal format conversion if input doesn't match
        SwsContext *m_swsContext = nullptr;
        int m_swsWidth = 0;
        int m_swsHeight = 0;
        AVPixelFormat m_swsInFmt = AV_PIX_FMT_NONE;
        AVPixelFormat m_swsOutFmt = AV_PIX_FMT_NONE;
    };

} // namespace pb

#endif // VIDEOENCODER_H
