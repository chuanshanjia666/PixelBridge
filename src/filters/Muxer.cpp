#include "filters/Muxer.h"
#include <spdlog/spdlog.h>

namespace pb
{

    Muxer::Muxer(const std::string &url) : Filter("Muxer"), m_url(url) {}

    Muxer::~Muxer()
    {
        stop();
        if (m_formatCtx)
        {
            if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE))
            {
                avio_closep(&m_formatCtx->pb);
            }
            avformat_free_context(m_formatCtx);
        }
    }

    bool Muxer::initialize(AVCodecContext *encoderCtx)
    {
        const char *formatName = nullptr;
        if (m_url.find("rtmp://") == 0)
            formatName = "flv";
        else if (m_url.find("rtsp://") == 0)
            formatName = "rtsp";
        else if (m_url.find("rtp://") == 0)
        {
            // 对于 RTP，默认使用 MPEG-TS 封装，这样接收端更容易处理（不需要 SDP）
            formatName = "rtp_mpegts";
        }
        else if (m_url.find("udp://") == 0)
        {
            formatName = "mpegts";
        }

        if (avformat_alloc_output_context2(&m_formatCtx, nullptr, formatName, m_url.c_str()) < 0)
        {
            spdlog::error("Could not create output context for {}", m_url);
            return false;
        }

        // RTSP 特定配置
        AVDictionary *options = nullptr;
        if (m_url.find("rtsp://") == 0)
        {
            // 强制使用 TCP 传输，避免 UDP 丢包导致花屏
            av_dict_set(&options, "rtsp_transport", "tcp", 0);
            // 设置超时时间
            av_dict_set(&options, "stimeout", "5000000", 0);
            spdlog::info("RTSP muxer configured with TCP transport");
        }

        // 通用的极致低延迟配置
        av_dict_set(&options, "buffer_size", "1024", 0);
        av_dict_set(&options, "flush_packets", "1", 0);
        av_dict_set(&options, "tune", "zerolatency", 0);

        m_outStream = avformat_new_stream(m_formatCtx, nullptr);
        if (!m_outStream)
        {
            spdlog::error("Could not create new stream");
            return false;
        }

        if (avcodec_parameters_from_context(m_outStream->codecpar, encoderCtx) < 0)
        {
            spdlog::error("Could not copy codec parameters to muxer");
            return false;
        }

        m_srcTimeBase = encoderCtx->time_base;

        if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE))
        {
            if (avio_open(&m_formatCtx->pb, m_url.c_str(), AVIO_FLAG_WRITE) < 0)
            {
                spdlog::error("Could not open output URL: {}", m_url);
                return false;
            }
        }

        if (avformat_write_header(m_formatCtx, &options) < 0)
        {
            spdlog::error("Error occurred when opening output URL");
            if (options)
                av_dict_free(&options);
            return false;
        }

        if (options)
            av_dict_free(&options);
        spdlog::info("Muxer initialized for URL: {}", m_url);
        return true;
    }

    void Muxer::process(DataPacket::Ptr packet)
    {
        if (packet->type() != PacketType::AV_PACKET)
            return;

        auto pktWrapper = std::static_pointer_cast<AVPacketWrapper>(packet);
        AVPacket *pkt = pktWrapper->get();

        // Rescale timestamps from encoder timebase to muxer stream timebase
        av_packet_rescale_ts(pkt, m_srcTimeBase, m_outStream->time_base);
        pkt->stream_index = m_outStream->index;

        static int muxLog = 0;
        if (++muxLog % 60 == 0)
        {
            spdlog::info("[Muxer] Writing packet pts={}, size={} to {}", pkt->pts, pkt->size, m_url);
        }

        if (av_interleaved_write_frame(m_formatCtx, pkt) < 0)
        {
            spdlog::error("Error while writing frame");
        }
    }

    void Muxer::stop()
    {
        if (m_formatCtx)
        {
            av_write_trailer(m_formatCtx);
        }
    }

} // namespace pb
