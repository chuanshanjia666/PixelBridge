#include <iostream>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "core/Bridge.h"
#include "filters/Demuxer.h"
#include "filters/VideoDecoder.h"
#include "filters/VideoSink.h"
#include "filters/ScreenCapture.h"
#include "filters/VideoEncoder.h"
#include "filters/Muxer.h"
#include "filters/RtspServerFilter.h"

extern "C"
{
#include <libavformat/avformat.h>
}

// Global functions for CLI mode
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

void run_pusher(const std::string &inputUrl, const std::string &outputUrl, const std::string &encoderName, const std::string &hwType)
{
    std::shared_ptr<pb::Filter> source;
    AVCodecParameters *codecParams = nullptr;
    if (inputUrl.find("screen") == 0)
    {
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
    std::this_thread::sleep_for(std::chrono::seconds(100));
    source->stop();
}

void run_server(const std::string &inputUrl, int port, const std::string &streamName, const std::string &encoderName, const std::string &hwType)
{
    std::shared_ptr<pb::Filter> source;
    AVCodecParameters *codecParams = nullptr;
    if (inputUrl.find("screen") == 0)
    {
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
    auto server = std::make_shared<pb::RtspServerFilter>(port, streamName);
    if (!server->initialize(encoder->getCodecContext()))
        return;
    source->setNextFilter(decoder.get());
    decoder->setNextFilter(encoder.get());
    encoder->setNextFilter(server.get());
    spdlog::info("Starting RTSP Server on port {} with stream name \x27{}\x27", port, streamName);
    source->start();
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
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
        spdlog::info("Launching GUI mode...");
        QQuickStyle::setStyle("Fusion");
        qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
        QGuiApplication app(argc, argv);
        Bridge bridge;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("bridge", &bridge);
        const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl)
                         {
            if (!obj && url == objUrl) QCoreApplication::exit(-1); }, Qt::QueuedConnection);
        engine.load(url);
        int ret = app.exec();
        avformat_network_deinit();
        return ret;
    }

    std::string mode = argv[1];
    if (mode == "play" && argc >= 3)
    {
        run_player(argv[2], (argc > 3) ? argv[3] : "");
    }
    else if (mode == "push" && argc >= 4)
    {
        run_pusher(argv[2], argv[3], (argc > 4) ? argv[4] : "libx264", (argc > 5) ? argv[5] : "");
    }
    else if (mode == "serve" && argc >= 3)
    {
        run_server(argv[2], (argc > 3) ? std::stoi(argv[3]) : 8554, (argc > 4) ? argv[4] : "live", (argc > 5) ? argv[5] : "libx264", (argc > 6) ? argv[6] : "");
    }
    else
    {
        spdlog::error("Invalid arguments.");
    }
    avformat_network_deinit();
    return 0;
}
