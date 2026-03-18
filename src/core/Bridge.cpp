#include "core/Bridge.h"
#include "filters/Demuxer.h"
#include "filters/VideoDecoder.h"
#include "filters/VideoEncoder.h"
#include "filters/RtspServerFilter.h"
#include "filters/Muxer.h"
#if !defined(Q_OS_ANDROID)
#include "filters/ScreenCapture.h"
#endif
#include <thread>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
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
    spdlog::info("Bridge destructor started");
    stopAll();
    spdlog::info("Bridge chains stopped, deleting QML sink");
    delete m_qmlSink;
    spdlog::info("Bridge destructor finished");
}

QVideoSink *Bridge::videoSink() const
{
    return m_qmlSink->videoSink();
}

void Bridge::setVideoSink(QVideoSink *sink)
{
    if (m_qmlSink->videoSink() != sink)
    {
        // Use a lock to prevent the pipeline from writing to the sink while it's being swapped
        std::lock_guard<std::mutex> lock(m_chainMutex);
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
    spdlog::info("stopAll() called, acquiring lock...");
    spdlog::default_logger()->flush();

    std::lock_guard<std::mutex> lock(m_chainMutex);
    spdlog::info("stopAll() lock acquired, stopping {} chains", m_chains.size());
    spdlog::default_logger()->flush();

    // Reverse order stop: Source filters first to stop data flow, then others
    for (size_t i = 0; i < m_chains.size(); ++i)
    {
        auto &chain = m_chains[i];
        if (!chain.empty())
        {
            spdlog::info("Stopping source for chain {}", i);
            spdlog::default_logger()->flush();
            // Stop source first
            chain[0]->stop();
        }
    }

    for (size_t i = 0; i < m_chains.size(); ++i)
    {
        auto &chain = m_chains[i];
        spdlog::info("Stopping remaining filters for chain {}", i);
        spdlog::default_logger()->flush();
        // Stop the rest of filters in reverse order (Muxer/Server last to flush trailers/close)
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            spdlog::info("Stopping filter: {}", (*it)->name());
            spdlog::default_logger()->flush();
            (*it)->stop();
        }
    }

    m_chains.clear();
    spdlog::info("All pipeline chains stopped and cleared.");
    spdlog::default_logger()->flush();
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

void Bridge::startServe(const QString &source, int port, const QString &name, const QString &encoder, const QString &hw, int fps, int latencyLevel, bool echo, const QString &address, int outputWidth, int outputHeight)
{
    stopAll();
    std::string sSource = source.toStdString();
    std::string sName = name.toStdString();
    std::string sEnc = encoder.toStdString();
    std::string sHw = hw.toStdString();
    std::string sAddr = address.toStdString();
    pb::LatencyLevel level = (pb::LatencyLevel)latencyLevel;

    std::thread([this, sSource, port, sName, sEnc, sHw, fps, level, echo, sAddr, outputWidth, outputHeight]()
                {
        std::shared_ptr<pb::Filter> src;
        AVCodecParameters *params = nullptr;
        if (sSource.find("screen") == 0) {
#if !defined(Q_OS_ANDROID)
            std::string display = (sSource.find(":") != std::string::npos) ? sSource.substr(sSource.find(":") + 1) : ":1";
            auto capture = std::make_shared<pb::ScreenCapture>(display, fps);
            capture->setLatencyLevel(level);
            if (!capture->initialize()) return;
            params = capture->getCodecParameters();
            src = capture;
#else
            // Screen capture is unavailable on Android (requires the MediaProjection API).
            spdlog::error("[Bridge] Screen capture (source='{}') is not supported on Android. "
                          "Use a network/file source instead.", sSource);
            return;
#endif
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
        int targetWidth = outputWidth > 0 ? outputWidth : params->width;
        int targetHeight = outputHeight > 0 ? outputHeight : params->height;
        if (!enc->initialize(targetWidth, targetHeight, fps)) return;

        auto server = std::make_shared<pb::RtspServerFilter>(port, sName, sAddr);
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
        
        spdlog::info("Starting RTSP server (Level: {}, Echo: {}): rtsp://{}:{}/{}", (int)level, echo, sAddr.empty() ? "localhost" : sAddr, port, sName);
        src->start(); })
        .detach();
}

void Bridge::startPush(const QString &input, const QString &output, const QString &encoder, const QString &hw, int fps, int latencyLevel, bool echo, int outputWidth, int outputHeight)
{
    stopAll();
    std::string sInput = input.toStdString();
    std::string sOutput = output.toStdString();
    std::string sEnc = encoder.toStdString();
    std::string sHw = hw.toStdString();
    pb::LatencyLevel level = (pb::LatencyLevel)latencyLevel;

    std::thread([this, sInput, sOutput, sEnc, sHw, fps, level, echo, outputWidth, outputHeight]()
                {
        std::shared_ptr<pb::Filter> src;
        AVCodecParameters *params = nullptr;
        if (sInput.find("screen") == 0) {
#if !defined(Q_OS_ANDROID)
            std::string display = (sInput.find(":") != std::string::npos) ? sInput.substr(sInput.find(":") + 1) : ":1";
            auto capture = std::make_shared<pb::ScreenCapture>(display, fps);
            capture->setLatencyLevel(level);
            if (!capture->initialize()) return;
            params = capture->getCodecParameters();
            src = capture;
#else
            // Screen capture is unavailable on Android (requires the MediaProjection API).
            spdlog::error("[Bridge] Screen capture (source='{}') is not supported on Android. "
                          "Use a network/file source instead.", sInput);
            return;
#endif
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
        int targetWidth = outputWidth > 0 ? outputWidth : params->width;
        int targetHeight = outputHeight > 0 ? outputHeight : params->height;
        if (!enc->initialize(targetWidth, targetHeight, fps)) return;

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
#if defined(Q_OS_ANDROID)
    if (url.scheme() == "content")
    {
        const QString imported = importAndroidContentToCache(url);
        if (!imported.isEmpty())
        {
            return imported;
        }

        // Fallback for Android content URI: some Qt/FFmpeg builds can still open it.
        return url.toString();
    }
#endif

    return url.toLocalFile();
}

QString Bridge::importAndroidContentToCache(const QUrl &url)
{
#if !defined(Q_OS_ANDROID)
    Q_UNUSED(url)
    return QString();
#else
    const QString key = url.toString();
    {
        std::lock_guard<std::mutex> lock(m_androidCacheMutex);
        auto it = m_androidImportedFiles.find(key.toStdString());
        if (it != m_androidImportedFiles.end())
        {
            const QString existing = QString::fromStdString(it->second);
            if (QFileInfo::exists(existing))
            {
                return existing;
            }
            m_androidImportedFiles.erase(it);
        }
    }

    QFile src(key);
    if (!src.open(QIODevice::ReadOnly))
    {
        spdlog::error("[Bridge] Failed to open Android content URI: {}", key.toStdString());
        return QString();
    }

    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (cacheRoot.isEmpty())
    {
        cacheRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (cacheRoot.isEmpty())
    {
        spdlog::error("[Bridge] No writable cache path found for Android imported media");
        return QString();
    }

    const QString dirPath = cacheRoot + "/imported_media";
    QDir dir;
    if (!dir.mkpath(dirPath))
    {
        spdlog::error("[Bridge] Failed to create media cache dir: {}", dirPath.toStdString());
        return QString();
    }

    QString suffix = QFileInfo(url.path()).suffix();
    if (!suffix.isEmpty())
    {
        suffix = "." + suffix;
    }
    else
    {
        suffix = ".bin";
    }

    const QString safeName = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QRegularExpression("[^A-Za-z0-9_-]"));
    const QString dstPath = dirPath + "/" + safeName + suffix;

    QFile dst(dstPath);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        spdlog::error("[Bridge] Failed to create imported file: {}", dstPath.toStdString());
        return QString();
    }

    constexpr qint64 kChunkSize = 1 << 20;
    while (!src.atEnd())
    {
        const QByteArray chunk = src.read(kChunkSize);
        if (chunk.isEmpty() && src.error() != QFileDevice::NoError)
        {
            spdlog::error("[Bridge] Failed while reading Android content URI: {}", key.toStdString());
            dst.remove();
            return QString();
        }
        if (!chunk.isEmpty() && dst.write(chunk) != chunk.size())
        {
            spdlog::error("[Bridge] Failed while writing imported media file: {}", dstPath.toStdString());
            dst.remove();
            return QString();
        }
    }

    dst.close();
    src.close();

    {
        std::lock_guard<std::mutex> lock(m_androidCacheMutex);
        m_androidImportedFiles[key.toStdString()] = dstPath.toStdString();
    }

    spdlog::info("[Bridge] Imported Android content URI to {}", dstPath.toStdString());
    return dstPath;
#endif
}
