#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include "core/Filter.h"

namespace pb {

class VideoDecoder : public Filter {
public:
    VideoDecoder(AVCodecParameters* params, const std::string& hwTypeName = "");
    ~VideoDecoder();

    bool initialize() override;
    void process(DataPacket::Ptr packet) override;
    void stop() override;

private:
    static enum AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts);
    bool init_hw_decoder(AVHWDeviceType type);

    AVCodecParameters* m_codecParams = nullptr;
    AVCodecContext* m_codecCtx = nullptr;
    const AVCodec* m_codec = nullptr;
    
    AVBufferRef* m_hwDeviceCtx = nullptr;
    AVHWDeviceType m_hwType = AV_HWDEVICE_TYPE_NONE;
};

} // namespace pb

#endif // VIDEODECODER_H
