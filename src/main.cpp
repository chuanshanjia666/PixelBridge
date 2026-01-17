#include <iostream>
#include "filters/Demuxer.h"
#include "filters/VideoDecoder.h"
#include "filters/VideoSink.h"
#include "filters/ScreenCapture.h"
#include "filters/VideoEncoder.h"
#include "filters/Muxer.h"
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

void run_player(const std::string &url, const std::string &hwType)
{
    auto demuxer = std::make_shared<pb::Demuxer>(url);
    if (!demuxer->initialize())
        return;

    auto decoder = std::make_shared<pb::VideoDecoder>(demuxer->getVideoCodecParameters(), hwType);
    if (!decoder->initialize())
        return;

    auto sink = std::make_shared<pb::VideoSink>();
    if (!sink->initialize())
        return;

    demuxer->setNextFilter(decoder.get());
    decoder->setNextFilter(sink.get());

    spdlog::info("Starting playback: {}", url);
    demuxer->start();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    demuxer->stop();
}

void run_pusher(const std::string &inputUrl, const std::string &outputUrl, const std::string &encoderName = "libx264", const std::string &hwType = "")
{
    std::shared_ptr<pb::Filter> source;
    AVCodecParameters *codecParams = nullptr;

    // 根据输入地址判断源类型
    if (inputUrl.find("screen") == 0)
    {
        // 格式支持 "screen" (默认:1) 或 "screen::0"
        std::string display = (inputUrl.find(":") != std::string::npos) ? inputUrl.substr(inputUrl.find(":") + 1) : ":1";
        auto capture = std::make_shared<pb::ScreenCapture>(display);
        if (!capture->initialize())
            return;
        codecParams = capture->getCodecParameters();
        source = capture;
    }
    else
    {
        auto demuxer = std::make_shared<pb::Demuxer>(inputUrl);
        if (!demuxer->initialize())
            return;
        codecParams = demuxer->getVideoCodecParameters();
        source = demuxer;
    }

    auto decoder = std::make_shared<pb::VideoDecoder>(codecParams, hwType);
    if (!decoder->initialize())
        return;

    auto encoder = std::make_shared<pb::VideoEncoder>(encoderName, hwType);
    if (!encoder->initialize(codecParams->width, codecParams->height))
        return;

    auto muxer = std::make_shared<pb::Muxer>(outputUrl);
    if (!muxer->initialize(encoder->getCodecContext()))
        return;

    source->setNextFilter(decoder.get());
    decoder->setNextFilter(encoder.get());
    encoder->setNextFilter(muxer.get());

    spdlog::info("Starting push: {} -> {} (Encoder: {})", inputUrl, outputUrl, encoderName);
    source->start();
    std::this_thread::sleep_for(std::chrono::seconds(100)); // 推流 100 秒
    source->stop();
}

int main(int argc, char *argv[])
{
    spdlog::set_level(spdlog::level::debug);

    if (avformat_network_init() < 0)
    {
        spdlog::critical("Failed to initialize network");
        return 1;
    }

    if (argc < 2)
    {
        spdlog::info("Usage:");
        spdlog::info("  Play:  {} play <url|file> [hw_type]", argv[0]);
        spdlog::info("  Push:  {} push <source> <target> [encoder] [hw_type]", argv[0]);
        spdlog::info("  Note:  <source> 可以是文件路径、网络流(rtsp/rtmp) 或 'screen[:display]'");
        spdlog::info("  Example 1: {} push test.mp4 udp://127.0.0.1:1234 h264_nvenc cuda", argv[0]);
        spdlog::info("  Example 2: {} push screen udp://127.0.0.1:1234 h264_nvenc cuda", argv[0]);
        avformat_network_deinit();
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "play" && argc >= 3)
    {
        std::string url = argv[2];
        std::string hwType = (argc > 3) ? argv[3] : "";
        run_player(url, hwType);
    }
    else if (mode == "push" && argc >= 4)
    {
        std::string inputUrl = argv[2];
        std::string outputUrl = argv[3];
        std::string encoder = (argc > 4) ? argv[4] : "libx264";
        std::string hwType = (argc > 5) ? argv[5] : "";
        run_pusher(inputUrl, outputUrl, encoder, hwType);
    }
    else
    {
        spdlog::error("Invalid arguments. Use no arguments for help.");
    }

    avformat_network_deinit();
    return 0;
}
