#include "filters/ScreenCapture.h"
#include <spdlog/spdlog.h>

namespace pb
{

    ScreenCapture::ScreenCapture(const std::string &display)
        : Filter("ScreenCapture"), m_display(display)
    {
        avdevice_register_all();
    }

    ScreenCapture::~ScreenCapture()
    {
        stop();
        if (m_formatCtx)
        {
            avformat_close_input(&m_formatCtx);
        }
    }

    bool ScreenCapture::initialize()
    {
        // Try x11grab (via XWayland on Wayland)
        const AVInputFormat *inputFmt = av_find_input_format("x11grab");
        if (!inputFmt)
        {
            spdlog::error("x11grab input format not found");
            return false;
        }

        AVDictionary *options = nullptr;
        av_dict_set(&options, "framerate", "30", 0);
        av_dict_set(&options, "video_size", "1920x1080", 0); // Default to 1080p

        // m_display is usually :0.0 for X11/XWayland
        if (avformat_open_input(&m_formatCtx, m_display.c_str(), inputFmt, &options) != 0)
        {
            spdlog::error("Could not open x11grab (XWayland) device: {}", m_display);
            spdlog::info("Hint: Make sure DISPLAY is set correctly (usually :0 or :1)");
            return false;
        }

        if (avformat_find_stream_info(m_formatCtx, nullptr) < 0)
        {
            spdlog::error("Could not find stream information for ScreenCapture");
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
            spdlog::error("Could not find video stream in ScreenCapture");
            return false;
        }

        spdlog::info("ScreenCapture initialized using x11grab on display: {}", m_display);
        return true;
    }

    void ScreenCapture::process(DataPacket::Ptr packet)
    {
        // Source filter
    }

    void ScreenCapture::start()
    {
        m_running = true;
        m_thread = std::thread(&ScreenCapture::run, this);
    }

    void ScreenCapture::stop()
    {
        m_running = false;
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    AVCodecParameters *ScreenCapture::getCodecParameters() const
    {
        if (m_videoStreamIndex == -1)
            return nullptr;
        return m_formatCtx->streams[m_videoStreamIndex]->codecpar;
    }

    void ScreenCapture::run()
    {
        while (m_running)
        {
            auto pktWrapper = std::make_shared<AVPacketWrapper>();
            if (av_read_frame(m_formatCtx, pktWrapper->get()) >= 0)
            {
                if (pktWrapper->get()->stream_index == m_videoStreamIndex)
                {
                    if (m_next)
                    {
                        m_next->process(pktWrapper);
                    }
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

} // namespace pb
