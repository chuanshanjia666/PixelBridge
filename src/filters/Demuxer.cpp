#include "filters/Demuxer.h"
#include <iostream>
#include <chrono>
#include <spdlog/spdlog.h>

namespace pb
{

    Demuxer::Demuxer(const std::string &url) : Filter("Demuxer"), m_url(url) {}

    Demuxer::~Demuxer()
    {
        spdlog::info("[Demuxer] Destructor started");
        spdlog::default_logger()->flush();
        stop();
        if (m_formatCtx)
        {
            spdlog::info("[Demuxer] Closing format context...");
            spdlog::default_logger()->flush();
            avformat_close_input(&m_formatCtx);
            spdlog::info("[Demuxer] Format context closed.");
            spdlog::default_logger()->flush();
        }
        spdlog::info("[Demuxer] Destructor finished");
        spdlog::default_logger()->flush();
    }

    bool Demuxer::initialize()
    {
        AVDictionary *options = nullptr;

        // 根据延迟等级配置参数
        if (m_latencyLevel == LatencyLevel::UltraLow)
        {
            av_dict_set(&options, "probesize", "32000", 0);
            av_dict_set(&options, "analyzeduration", "50000", 0);
            av_dict_set(&options, "fflags", "nobuffer", 0);
            av_dict_set(&options, "flags", "low_delay", 0);
        }
        else if (m_latencyLevel == LatencyLevel::Low)
        {
            av_dict_set(&options, "probesize", "200000", 0);
            av_dict_set(&options, "analyzeduration", "500000", 0);
            av_dict_set(&options, "fflags", "nobuffer", 0);
        }
        else
        {
            // Standard - 使用较保守的参数，提高复杂流的稳定性
            av_dict_set(&options, "probesize", "1000000", 0);
            av_dict_set(&options, "analyzeduration", "1000000", 0);
        }

        // Check if it's a UDP stream and apply specific options
        if (m_url.find("udp://") == 0)
        {
            spdlog::info("Detected UDP stream, applying network options...");
            av_dict_set(&options, "fifo_size", "1000000", 0);
            av_dict_set(&options, "buffer_size", "1000000", 0);
            av_dict_set(&options, "overrun_nonfatal", "1", 0);
            av_dict_set(&options, "timeout", "5000000", 0); // 5 seconds timeout
        }
        else if (m_url.find("rtsp://") == 0)
        {
            spdlog::info("Detected RTSP stream, applying low latency options...");
            // 使用 UDP 传输以获得最小延迟
            av_dict_set(&options, "rtsp_transport", "udp", 0);
            av_dict_set(&options, "stimeout", "5000000", 0);
        }

        if (avformat_open_input(&m_formatCtx, m_url.c_str(), nullptr, &options) != 0)
        {
            spdlog::error("Could not open input: {}", m_url);
            if (options)
                av_dict_free(&options);
            return false;
        }

        if (options)
            av_dict_free(&options);

        if (avformat_find_stream_info(m_formatCtx, nullptr) < 0)
        {
            spdlog::error("Could not find stream information");
            return false;
        }

        for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++)
        {
            if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                m_videoStreamIndex = i;
                break;
            }
        }

        if (m_videoStreamIndex == -1)
        {
            spdlog::error("Could not find video stream");
            return false;
        }

        spdlog::info("Demuxer initialized for URL: {}", m_url);
        if (m_formatCtx->iformat)
        {
            spdlog::info("Input format: {}", m_formatCtx->iformat->long_name);
        }
        return true;
    }

    void Demuxer::process(DataPacket::Ptr packet)
    {
        // Demuxer is a source filter, it doesn't process incoming packets.
    }

    void Demuxer::start()
    {
        m_running = true;
        m_thread = std::thread(&Demuxer::run, this);
    }

    AVCodecParameters *Demuxer::getVideoCodecParameters() const
    {
        if (m_videoStreamIndex == -1)
            return nullptr;
        return m_formatCtx->streams[m_videoStreamIndex]->codecpar;
    }

    void Demuxer::stop()
    {
        spdlog::info("[Demuxer] stop() called for {}", m_url);
        m_running = false;
        // Break FFmpeg blocking call
        if (m_formatCtx)
        {
            // Note: avformat_close_input will handle it, but for real-time streams
            // we occasionally need a callback or interrupt.
            // Simplified: we rely on m_running check after av_read_frame.
        }

        if (m_thread.joinable())
        {
            spdlog::info("[Demuxer] Joining thread for {}", m_url);
            m_thread.join();
            spdlog::info("[Demuxer] Thread joined for {}", m_url);
        }
    }

    void Demuxer::run()
    {
        auto startTime = std::chrono::steady_clock::now();
        int64_t firstTimestamp = AV_NOPTS_VALUE;
        AVRational timeBase = {1, 1000};
        if (m_videoStreamIndex >= 0 && m_videoStreamIndex < (int)m_formatCtx->nb_streams)
        {
            timeBase = m_formatCtx->streams[m_videoStreamIndex]->time_base;
        }

        // Determine if we should pace.
        // Generally, if it's a network stream, we don't pace because it's already real-time.
        // If it's a local file, we must pace.
        bool shouldPace = true;
        if (m_url.find("rtsp://") == 0 || m_url.find("udp://") == 0 || m_url.find("rtp://") == 0)
        {
            shouldPace = false;
        }

        while (m_running)
        {
            auto pktWrapper = std::make_shared<AVPacketWrapper>();
            if (av_read_frame(m_formatCtx, pktWrapper->get()) >= 0)
            {
                if (pktWrapper->get()->stream_index == m_videoStreamIndex)
                {
                    if (shouldPace)
                    {
                        int64_t ts = pktWrapper->get()->dts;
                        if (ts == AV_NOPTS_VALUE)
                            ts = pktWrapper->get()->pts;

                        if (ts != AV_NOPTS_VALUE)
                        {
                            if (firstTimestamp == AV_NOPTS_VALUE)
                            {
                                firstTimestamp = ts;
                                startTime = std::chrono::steady_clock::now();
                            }
                            else
                            {
                                auto now = std::chrono::steady_clock::now();
                                double elapsed = std::chrono::duration<double>(now - startTime).count();
                                double target = (ts - firstTimestamp) * av_q2d(timeBase);

                                if (target > elapsed)
                                {
                                    std::this_thread::sleep_for(std::chrono::duration<double>(target - elapsed));
                                }
                            }
                        }
                    }

                    if (m_next)
                    {
                        static int demuxLog = 0;
                        if (++demuxLog % 60 == 0)
                        {
                            spdlog::info("[Demuxer] Read packet pts={} from {}", pktWrapper->get()->pts, m_url);
                        }
                        m_next->process(pktWrapper);
                    }
                }
            }
            else
            {
                // EOF or error
                m_running = false;
            }
        }
    }

} // namespace pb
