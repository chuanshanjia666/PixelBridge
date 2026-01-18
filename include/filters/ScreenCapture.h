#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include "core/Filter.h"
#include <QObject>
#include <QScreenCapture>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

extern "C"
{
#include <libavcodec/avcodec.h>
}

namespace pb
{

    class ScreenCapture : public QObject, public Filter
    {
        Q_OBJECT
    public:
        ScreenCapture(const std::string &display = ":0.0", int fps = 30);
        ~ScreenCapture();

        bool initialize() override;
        void process(DataPacket::Ptr packet) override {}
        void stop() override;
        void start() override;

        AVCodecParameters *getCodecParameters() const;

    private slots:
        void handleFrame(const QVideoFrame &frame);

    private:
        void workerThread();

        std::string m_display;
        int m_fps;
        int64_t m_lastFrameTime = 0;
        int64_t m_frameCount = 0;
        bool m_initialized = false;
        QScreenCapture *m_screenCapture = nullptr;
        QMediaCaptureSession *m_captureSession = nullptr;
        QVideoSink *m_videoSink = nullptr;
        AVCodecParameters *m_codecParams = nullptr;

        std::thread m_worker;
        std::atomic<bool> m_running{false};

        struct RawFrame
        {
            std::vector<uint8_t> data;
            int width = 0;
            int height = 0;
            int bytesPerLine = 0;
            QVideoFrameFormat::PixelFormat pixelFormat = QVideoFrameFormat::Format_Invalid;
        };
        std::queue<RawFrame> m_frameQueue;
        RawFrame m_sharedBuffer; // 为了减少内存分配，重用此 buffer
        std::mutex m_queueMutex;
        std::condition_variable m_cv;
    };

} // namespace pb

#endif // SCREENCAPTURE_H
