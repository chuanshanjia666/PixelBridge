#include "core/Bridge.h"
#include "filters/Demuxer.h"
#include "filters/VideoDecoder.h"
#include "filters/VideoEncoder.h"
#include "filters/RtspServerFilter.h"
#include "filters/Muxer.h"
#include "filters/ScreenCapture.h"
#include <thread>
#include <QUrl>
#include <spdlog/spdlog.h>
extern "C"
{
#include <libavutil/hwcontext.h>
}
namespace pb
{
    class TeeFilter : public Filter
    {
    public:
        TeeFilter() : Filter("TeeFilter") {}
        void addTarget(Filter *target) { m_targets.push_back(target); }
        bool initialize() override { return true; }
        void process(DataPacket::Ptr packet) override
        {
            static int teeLog = 0;
            if (++teeLog % 60 == 0)
            {
                spdlog::info("[TeeFilter] Distributing packet to {} targets", m_targets.size());
            }
            for (auto target : m_targets)
            {
                target->process(packet);
            }
        }
        void stop() override {}

    private:
        std::vector<Filter *> m_targets;
    };
}

Bridge::Bridge(QObject *parent) : QObject(parent)
{
    m_qmlSink = new pb::QmlVideoSinkFilter();
}

Bridge::~Bridge()
{
    // Basic cleanup
}

QVideoSink *Bridge::videoSink() const
{
    return m_qmlSink->videoSink();
}

void Bridge::setVideoSink(QVideoSink *sink)
{
    if (m_qmlSink->videoSink() != sink)
    {
        m_qmlSink->setVideoSink(sink);
        emit videoSinkChanged();
    }
}

QStringList Bridge::hwTypes() const
{
    QStringList types;
    types << "None";
    AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
    {
        types << QString::fromStdString(av_hwdevice_get_type_name(type));
    }
    return types;
}

void Bridge::stopAll()
{
    std::lock_guard<std::mutex> lock(m_chainMutex);
    for (auto &chain : m_chains)
    {
        if (!chain.empty() && chain[0])
        {
            chain[0]->stop(); // 停止源头（如 ScreenCapture 线程）
        }
        for (auto &filter : chain)
        {
            filter->stop();
        }
    }
    m_chains.clear();
    spdlog::info("All pipeline chains stopped and cleared.");
}

void Bridge::startPlay(const QString &url, const QString &hwType)
{
    stopAll();
    std::string sUrl = url.toStdString();
    std::string sHw = hwType.toStdString();
    std::thread([this, sUrl, sHw]()
                {
        auto demuxer = std::make_shared<pb::Demuxer>(sUrl);
        if (!demuxer->initialize()) return;
        auto decoder = std::make_shared<pb::VideoDecoder>(demuxer->getVideoCodecParameters(), sHw);
        if (!decoder->initialize()) return;
        
        demuxer->setNextFilter(decoder.get());
        decoder->setNextFilter(m_qmlSink);
        
        {
            std::lock_guard<std::mutex> lock(m_chainMutex);
            m_chains.push_back({demuxer, decoder});
        }
        
        spdlog::info("Starting playback to QML: {}", sUrl);
        demuxer->start(); })
        .detach();
}

void Bridge::startServe(const QString &source, int port, const QString &name, const QString &encoder, const QString &hw, int fps)
{
    stopAll();
    std::string sSource = source.toStdString();
    std::string sName = name.toStdString();
    std::string sEnc = encoder.toStdString();
    std::string sHw = hw.toStdString();
    std::thread([this, sSource, port, sName, sEnc, sHw, fps]()
                {
        std::shared_ptr<pb::Filter> src;
        AVCodecParameters *params = nullptr;
        if (sSource.find("screen") == 0) {
            std::string display = (sSource.find(":") != std::string::npos) ? sSource.substr(sSource.find(":") + 1) : ":1";
            auto capture = std::make_shared<pb::ScreenCapture>(display, fps);
            if (!capture->initialize()) return;
            params = capture->getCodecParameters();
            src = capture;
        } else {
            auto demuxer = std::make_shared<pb::Demuxer>(sSource);
            if (!demuxer->initialize()) return;
            params = demuxer->getVideoCodecParameters();
            src = demuxer;
        }
        auto decoder = std::make_shared<pb::VideoDecoder>(params, sHw);
        if (!decoder->initialize()) return;
        auto enc = std::make_shared<pb::VideoEncoder>(sEnc, sHw);
        if (!enc->initialize(params->width, params->height, fps)) return;
        auto server = std::make_shared<pb::RtspServerFilter>(port, sName);
        if (!server->initialize(enc->getCodecContext())) return;
        
        auto tee = std::make_shared<pb::TeeFilter>();
        tee->addTarget(enc.get());
        tee->addTarget(m_qmlSink);

        src->setNextFilter(decoder.get());
        decoder->setNextFilter(tee.get());
        enc->setNextFilter(server.get());
        
        {
            std::lock_guard<std::mutex> lock(m_chainMutex);
            m_chains.push_back({src, decoder, enc, server, tee});
        }
        
        spdlog::info("Starting RTSP server: rtsp://localhost:{}/{}", port, sName);
        src->start(); })
        .detach();
}

void Bridge::startPush(const QString &input, const QString &output, const QString &encoder, const QString &hw, int fps)
{
    stopAll();
    std::string sInput = input.toStdString();
    std::string sOutput = output.toStdString();
    std::string sEnc = encoder.toStdString();
    std::string sHw = hw.toStdString();
    std::thread([this, sInput, sOutput, sEnc, sHw, fps]()
                {
        std::shared_ptr<pb::Filter> src;
        AVCodecParameters *params = nullptr;
        if (sInput.find("screen") == 0) {
            std::string display = (sInput.find(":") != std::string::npos) ? sInput.substr(sInput.find(":") + 1) : ":1";
            auto capture = std::make_shared<pb::ScreenCapture>(display, fps);
            if (!capture->initialize()) return;
            params = capture->getCodecParameters();
            src = capture;
        } else {
            auto demuxer = std::make_shared<pb::Demuxer>(sInput);
            if (!demuxer->initialize()) return;
            params = demuxer->getVideoCodecParameters();
            src = demuxer;
        }
        auto decoder = std::make_shared<pb::VideoDecoder>(params, sHw);
        if (!decoder->initialize()) return;
        auto enc = std::make_shared<pb::VideoEncoder>(sEnc, sHw);
        if (!enc->initialize(params->width, params->height, fps)) return;
        auto muxer = std::make_shared<pb::Muxer>(sOutput);
        if (!muxer->initialize(enc->getCodecContext())) return;
        
        auto tee = std::make_shared<pb::TeeFilter>();
        tee->addTarget(enc.get());
        tee->addTarget(m_qmlSink);

        src->setNextFilter(decoder.get());
        decoder->setNextFilter(tee.get());
        enc->setNextFilter(muxer.get());
        
        {
            std::lock_guard<std::mutex> lock(m_chainMutex);
            m_chains.push_back({src, decoder, enc, muxer, tee});
        }
        
        spdlog::info("Starting push and QML preview: {} -> {}", sInput, sOutput);
        src->start(); })
        .detach();
}

QString Bridge::urlToPath(const QUrl &url)
{
    return url.toLocalFile();
}
