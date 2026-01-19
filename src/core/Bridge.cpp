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
#include <libavcodec/avcodec.h>
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

QStringList Bridge::getEncoders(const QString &codecType, const QString &hwType)
{
    QStringList encoders;
    AVCodecID targetId = AV_CODEC_ID_NONE;
    if (codecType == "H.264")
        targetId = AV_CODEC_ID_H264;
    else if (codecType == "H.265")
        targetId = AV_CODEC_ID_HEVC;
    else if (codecType == "MJPEG")
        targetId = AV_CODEC_ID_MJPEG;
    else if (codecType == "AV1")
        targetId = AV_CODEC_ID_AV1;

    if (targetId == AV_CODEC_ID_NONE)
        return encoders;

    const AVCodec *codec = nullptr;
    void *opaque = nullptr;
    while ((codec = av_codec_iterate(&opaque)))
    {
        if (!av_codec_is_encoder(codec) || codec->id != targetId)
            continue;

        QString name = QString::fromUtf8(codec->name);

        if (hwType == "None")
        {
            // Show software encoders (usually don't have these suffixes)
            if (!name.contains("_nvenc") && !name.contains("_vaapi") &&
                !name.contains("_qsv") && !name.contains("_amf") &&
                !name.contains("_v4l2m2m") && !name.contains("_videotoolbox"))
            {
                encoders << name;
            }
        }
        else
        {
            // Find hardware matched encoders
            QString hwLower = hwType.toLower();
            bool match = false;
            if (hwLower == "cuda" && name.contains("nvenc"))
                match = true;
            else if (name.contains(hwLower))
                match = true;

            if (match)
            {
                encoders << name;
            }
        }
    }
    encoders.sort();
    if (hwType == "None")
    {
        if (codecType == "H.264" && encoders.contains("libx264"))
        {
            encoders.removeAll("libx264");
            encoders.prepend("libx264");
        }
        else if (codecType == "H.265" && encoders.contains("libx265"))
        {
            encoders.removeAll("libx265");
            encoders.prepend("libx265");
        }
    }
    return encoders;
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

void Bridge::startPlay(const QString &url, const QString &hwType, int latencyLevel)
{
    stopAll();
    std::string sUrl = url.toStdString();
    std::string sHw = hwType.toStdString();
    pb::LatencyLevel level = (pb::LatencyLevel)latencyLevel;

    std::thread([this, sUrl, sHw, level]()
                {
        auto demuxer = std::make_shared<pb::Demuxer>(sUrl);
        demuxer->setLatencyLevel(level);
        if (!demuxer->initialize()) return;

        auto decoder = std::make_shared<pb::VideoDecoder>(demuxer->getVideoCodecParameters(), sHw);
        decoder->setLatencyLevel(level);
        if (!decoder->initialize()) return;
        
        demuxer->setNextFilter(decoder.get());
        decoder->setNextFilter(m_qmlSink);
        
        {
            std::lock_guard<std::mutex> lock(m_chainMutex);
            m_chains.push_back({demuxer, decoder});
        }
        
        spdlog::info("Starting playback (Level: {}) to QML: {}", (int)level, sUrl);
        demuxer->start(); })
        .detach();
}

void Bridge::startServe(const QString &source, int port, const QString &name, const QString &encoder, const QString &hw, int fps, int latencyLevel, bool echo)
{
    stopAll();
    std::string sSource = source.toStdString();
    std::string sName = name.toStdString();
    std::string sEnc = encoder.toStdString();
    std::string sHw = hw.toStdString();
    pb::LatencyLevel level = (pb::LatencyLevel)latencyLevel;

    std::thread([this, sSource, port, sName, sEnc, sHw, fps, level, echo]()
                {
        std::shared_ptr<pb::Filter> src;
        AVCodecParameters *params = nullptr;
        if (sSource.find("screen") == 0) {
            std::string display = (sSource.find(":") != std::string::npos) ? sSource.substr(sSource.find(":") + 1) : ":1";
            auto capture = std::make_shared<pb::ScreenCapture>(display, fps);
            capture->setLatencyLevel(level);
            if (!capture->initialize()) return;
            params = capture->getCodecParameters();
            src = capture;
        } else {
            auto demuxer = std::make_shared<pb::Demuxer>(sSource);
            demuxer->setLatencyLevel(level);
            if (!demuxer->initialize()) return;
            params = demuxer->getVideoCodecParameters();
            src = demuxer;
        }

        auto decoder = std::make_shared<pb::VideoDecoder>(params, sHw);
        decoder->setLatencyLevel(level);
        if (!decoder->initialize()) return;

        auto enc = std::make_shared<pb::VideoEncoder>(sEnc, sHw);
        enc->setLatencyLevel(level);
        if (!enc->initialize(params->width, params->height, fps)) return;

        auto server = std::make_shared<pb::RtspServerFilter>(port, sName);
        server->setLatencyLevel(level);
        if (!server->initialize(enc->getCodecContext())) return;
        
        src->setNextFilter(decoder.get());
        std::vector<std::shared_ptr<pb::Filter>> filters = {src, decoder, enc, server};

        if (echo) {
            auto tee = std::make_shared<pb::TeeFilter>();
            tee->addTarget(enc.get());
            tee->addTarget(m_qmlSink);
            decoder->setNextFilter(tee.get());
            filters.push_back(tee);
        } else {
            decoder->setNextFilter(enc.get());
        }
        enc->setNextFilter(server.get());
        
        {
            std::lock_guard<std::mutex> lock(m_chainMutex);
            m_chains.push_back(filters);
        }
        
        spdlog::info("Starting RTSP server (Level: {}, Echo: {}): rtsp://localhost:{}/{}", (int)level, echo, port, sName);
        src->start(); })
        .detach();
}

void Bridge::startPush(const QString &input, const QString &output, const QString &encoder, const QString &hw, int fps, int latencyLevel, bool echo)
{
    stopAll();
    std::string sInput = input.toStdString();
    std::string sOutput = output.toStdString();
    std::string sEnc = encoder.toStdString();
    std::string sHw = hw.toStdString();
    pb::LatencyLevel level = (pb::LatencyLevel)latencyLevel;

    std::thread([this, sInput, sOutput, sEnc, sHw, fps, level, echo]()
                {
        std::shared_ptr<pb::Filter> src;
        AVCodecParameters *params = nullptr;
        if (sInput.find("screen") == 0) {
            std::string display = (sInput.find(":") != std::string::npos) ? sInput.substr(sInput.find(":") + 1) : ":1";
            auto capture = std::make_shared<pb::ScreenCapture>(display, fps);
            capture->setLatencyLevel(level);
            if (!capture->initialize()) return;
            params = capture->getCodecParameters();
            src = capture;
        } else {
            auto demuxer = std::make_shared<pb::Demuxer>(sInput);
            demuxer->setLatencyLevel(level);
            if (!demuxer->initialize()) return;
            params = demuxer->getVideoCodecParameters();
            src = demuxer;
        }

        auto decoder = std::make_shared<pb::VideoDecoder>(params, sHw);
        decoder->setLatencyLevel(level);
        if (!decoder->initialize()) return;

        auto enc = std::make_shared<pb::VideoEncoder>(sEnc, sHw);
        enc->setLatencyLevel(level);
        if (!enc->initialize(params->width, params->height, fps)) return;

        auto muxer = std::make_shared<pb::Muxer>(sOutput);
        muxer->setLatencyLevel(level);
        if (!muxer->initialize(enc->getCodecContext())) return;
        
        src->setNextFilter(decoder.get());
        std::vector<std::shared_ptr<pb::Filter>> filters = {src, decoder, enc, muxer};

        if (echo) {
            auto tee = std::make_shared<pb::TeeFilter>();
            tee->addTarget(enc.get());
            tee->addTarget(m_qmlSink);
            decoder->setNextFilter(tee.get());
            filters.push_back(tee);
        } else {
            decoder->setNextFilter(enc.get());
        }
        enc->setNextFilter(muxer.get());
        
        {
            std::lock_guard<std::mutex> lock(m_chainMutex);
            m_chains.push_back(filters);
        }
        
        spdlog::info("Starting push (Level: {}, Echo: {}): {} -> {}", (int)level, echo, sInput, sOutput);
        src->start(); })
        .detach();
}

QString Bridge::urlToPath(const QUrl &url)
{
    return url.toLocalFile();
}
