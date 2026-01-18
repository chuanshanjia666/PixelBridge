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

int main(int argc, char *argv[])
{
    spdlog::set_level(spdlog::level::debug);

    QGuiApplication app(argc, argv);

    if (avformat_network_init() < 0)
    {
        spdlog::critical("Failed to initialize network");
        return 1;
    }

    QQuickStyle::setStyle("Fusion");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");

    Bridge bridge;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("bridge", &bridge);
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl)
                     {
        if (!obj && url == objUrl) QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(url);

    if (argc >= 2)
    {
        std::string mode = argv[1];
        if (mode == "play" && argc >= 3)
        {
            bridge.startPlay(QString::fromStdString(argv[2]), (argc > 3) ? QString::fromStdString(argv[3]) : "");
        }
        else if (mode == "push" && argc >= 4)
        {
            bridge.startPush(QString::fromStdString(argv[2]), QString::fromStdString(argv[3]),
                             (argc > 4) ? QString::fromStdString(argv[4]) : "libx264",
                             (argc > 5) ? QString::fromStdString(argv[5]) : "");
        }
        else if (mode == "serve" && argc >= 3)
        {
            bridge.startServe(QString::fromStdString(argv[2]),
                              (argc > 3) ? std::stoi(argv[3]) : 8554,
                              (argc > 4) ? QString::fromStdString(argv[4]) : "live",
                              (argc > 5) ? QString::fromStdString(argv[5]) : "libx264",
                              (argc > 6) ? QString::fromStdString(argv[6]) : "");
        }
        else
        {
            spdlog::error("Invalid arguments. Use: play, push, or serve.");
        }
    }

    int ret = app.exec();
    avformat_network_deinit();
    return ret;
}
