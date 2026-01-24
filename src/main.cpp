#include <iostream>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDir>

#include "core/Bridge.h"
#include "core/Logger.h"
#include "core/Version.h"
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
    // Check for --version argument
    if (argc == 2 && std::string(argv[1]) == "--version")
    {
        std::cout << "PixelBridge version " << PIXELBRIDGE_VERSION << std::endl;
        return 0;
    }

    auto qmlSink = std::make_shared<QmlLogSinkMt>();
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto logger = std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{consoleSink, qmlSink});
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);

    QGuiApplication app(argc, argv);

    // 创建 QGuiApplication 后再设置环境变量
    QString exeDir;
#if defined(Q_OS_WIN)
    exeDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
#else
    exeDir = QCoreApplication::applicationDirPath();
#endif
    qputenv("QML2_IMPORT_PATH", (exeDir + "/qml").toUtf8());
    qputenv("QT_PLUGIN_PATH", (exeDir + "/plugins").toUtf8());

    spdlog::info("Running with platform: {}", qPrintable(app.platformName()));
    app.setWindowIcon(QIcon(":/assets/icons/pixelbridge.svg"));

    if (avformat_network_init() < 0)
    {
        spdlog::critical("Failed to initialize network");
        return 1;
    }

    QQuickStyle::setStyle("Fusion");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");

    int ret = 0;
    {
        Bridge bridge;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("bridge", &bridge);
        engine.rootContext()->setContextProperty("logModel", &Logger::instance());
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

        ret = app.exec();
        spdlog::info("Application event loop finished, stopping all bridge operations...");
        spdlog::default_logger()->flush();
        bridge.stopAll();

        spdlog::info("Destroying objects...");
        spdlog::default_logger()->flush();
    }

    spdlog::info("Deinitializing network...");
    spdlog::default_logger()->flush();
    avformat_network_deinit();
    spdlog::info("Main exiting with code {}", ret);
    spdlog::default_logger()->flush();
    return ret;
}
