#include "filters/VideoEncoder.h"
#include <spdlog/spdlog.h>

extern "C"
{
#include <libavutil/pixdesc.h>
}

namespace pb
{

    VideoEncoder::VideoEncoder(const std::string &codecName, const std::string &hwType)
        : Filter("VideoEncoder"), m_codecName(codecName), m_hwTypeName(hwType) {}

    VideoEncoder::~VideoEncoder()
    {
        stop();
        if (m_codecCtx)
        {
            avcodec_free_context(&m_codecCtx);
        }
        if (m_hwFramesCtx)
        {
            av_buffer_unref(&m_hwFramesCtx);
        }
        if (m_hwDeviceCtx)
        {
            av_buffer_unref(&m_hwDeviceCtx);
        }
    }

    bool VideoEncoder::init_hw_encoder()
    {
        AVHWDeviceType type = av_hwdevice_find_type_by_name(m_hwTypeName.c_str());
        if (type == AV_HWDEVICE_TYPE_NONE)
        {
            spdlog::error("Invalid hardware type: {}", m_hwTypeName);
            return false;
        }

        int err = av_hwdevice_ctx_create(&m_hwDeviceCtx, type, nullptr, nullptr, 0);
        if (err < 0)
        {
            spdlog::error("Failed to create HW device context for encoder");
            return false;
        }

        // Setup hw_frames_ctx
        m_hwFramesCtx = av_hwframe_ctx_alloc(m_hwDeviceCtx);
        if (!m_hwFramesCtx)
        {
            spdlog::error("Failed to allocate HW frame context");
            return false;
        }

        AVHWFramesContext *framesCtx = (AVHWFramesContext *)(m_hwFramesCtx->data);
        framesCtx->format = (m_hwTypeName == "cuda") ? AV_PIX_FMT_CUDA : AV_PIX_FMT_VAAPI;
        framesCtx->sw_format = AV_PIX_FMT_NV12;
        framesCtx->width = m_codecCtx->width;
        framesCtx->height = m_codecCtx->height;
        framesCtx->initial_pool_size = 20;

        if (av_hwframe_ctx_init(m_hwFramesCtx) < 0)
        {
            spdlog::error("Failed to initialize HW frame context");
            return false;
        }

        m_codecCtx->hw_frames_ctx = av_buffer_ref(m_hwFramesCtx);
        m_codecCtx->pix_fmt = framesCtx->format;

        return true;
    }

    bool VideoEncoder::initialize(int width, int height, int fps)
    {
        m_codec = avcodec_find_encoder_by_name(m_codecName.c_str());
        if (!m_codec)
        {
            spdlog::error("Encoder {} not found", m_codecName);
            return false;
        }

        m_codecCtx = avcodec_alloc_context3(m_codec);
        if (!m_codecCtx)
        {
            spdlog::error("Could not allocate encoder context");
            return false;
        }

        m_codecCtx->width = width;
        m_codecCtx->height = height;
        m_codecCtx->time_base = {1, fps};
        m_codecCtx->framerate = {fps, 1};
        m_codecCtx->gop_size = 30;    // 增大 GOP 周期
        m_codecCtx->max_b_frames = 0; // 彻底禁用 B 帧，解决 RTSP 解码 POC 错误

        if (!m_hwTypeName.empty())
        {
            if (init_hw_encoder())
            {
                spdlog::info("Using hardware encoder: {} with {}", m_codecName, m_hwTypeName);
                // Pixel format is already set in init_hw_encoder
            }
            else
            {
                return false;
            }
        }
        else
        {
            m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        }

        // 确保不使用全局头，而是将 SPS/PPS 放入每个 IDR 帧中
        // 这对于 RTSP 这种允许客户端随时加入的流非常重要
        m_codecCtx->flags &= ~AV_CODEC_FLAG_GLOBAL_HEADER;

        AVDictionary *options = nullptr;
        if (m_codecName == "libx264")
        {
            av_dict_set(&options, "preset", "veryfast", 0);
            av_dict_set(&options, "tune", "zerolatency", 0);
            // 强制开启重复头
            av_dict_set(&options, "x264-params", "repeat-headers=1:nal-hrd=cbr:force-cfr=1", 0);
        }
        else if (m_codecName.find("nvenc") != std::string::npos)
        {
            // NVENC 强制在每个关键帧重复 Header
            av_dict_set(&options, "forced-idr", "1", 0);
            av_dict_set(&options, "repeat_headers", "1", 0);
            av_dict_set(&options, "preset", "p1", 0); // 最快预设
            av_dict_set(&options, "tune", "ull", 0);  // 超低延迟
        }

        if (avcodec_open2(m_codecCtx, m_codec, &options) < 0)
        {
            spdlog::error("Could not open encoder");
            return false;
        }

        return true;
    }

    void VideoEncoder::process(DataPacket::Ptr packet)
    {
        if (packet->type() != PacketType::AV_FRAME)
            return;

        auto frameWrapper = std::static_pointer_cast<AVFrameWrapper>(packet);
        AVFrame *frame = frameWrapper->get();
        AVFrame *encodingFrame = frame;
        std::shared_ptr<AVFrameWrapper> hwFrameWrapper;

        // If we are using HW encoding but the input frame is in SW memory, we need to upload
        if (m_hwDeviceCtx && frame->format != m_codecCtx->pix_fmt)
        {
            static bool format_warning_logged = false;
            if (!format_warning_logged)
            {
                spdlog::info("Encoder input format: {}, converting/uploading to HW format: {}",
                             av_get_pix_fmt_name((AVPixelFormat)frame->format),
                             av_get_pix_fmt_name(m_codecCtx->pix_fmt));
                format_warning_logged = true;
            }

            hwFrameWrapper = std::make_shared<AVFrameWrapper>();
            AVFrame *hwFrame = hwFrameWrapper->get();
            hwFrame->format = m_codecCtx->pix_fmt;
            hwFrame->width = m_codecCtx->width;
            hwFrame->height = m_codecCtx->height;

            if (av_hwframe_get_buffer(m_codecCtx->hw_frames_ctx, hwFrame, 0) < 0)
            {
                spdlog::error("Failed to get HW frame buffer");
                return;
            }

            if (av_hwframe_transfer_data(hwFrame, frame, 0) < 0)
            {
                // 如果这里报错，说明像素格式不支持直接上传，需要 SwsContext 转换
                spdlog::error("Error transferring data to GPU. Frame format {} might be incompatible with HW format {}",
                              av_get_pix_fmt_name((AVPixelFormat)frame->format),
                              av_get_pix_fmt_name(m_codecCtx->pix_fmt));
                return;
            }
            hwFrame->pts = frame->pts;
            encodingFrame = hwFrame;
        }

        encodingFrame->pts = m_pts++;

        int ret = avcodec_send_frame(m_codecCtx, encodingFrame);
        if (ret < 0)
        {
            spdlog::error("Error sending frame for encoding");
            return;
        }

        while (ret >= 0)
        {
            auto pktWrapper = std::make_shared<AVPacketWrapper>();
            ret = avcodec_receive_packet(m_codecCtx, pktWrapper->get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                spdlog::error("Error during encoding");
                return;
            }

            if (m_next)
            {
                m_next->process(pktWrapper);
            }
        }
    }

    void VideoEncoder::stop()
    {
        // Flush encoder
        if (m_codecCtx)
        {
            avcodec_send_frame(m_codecCtx, nullptr);
        }
    }

} // namespace pb
