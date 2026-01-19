#include "filters/ScreenCapture.h"
#include <spdlog/spdlog.h>
#include <QGuiApplication>
#include <QScreen>
#include <QMetaObject>
#include <QThread>
#include <QDateTime>
#include <fstream>
#include <unistd.h>

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

namespace pb
{
    // 获取当前进程的内存统计
    static void get_memory_usage(long &vms, long &rss)
    {
        std::ifstream stat_stream("/proc/self/statm", std::ios_base::in);
        long vms_pages, rss_pages;
        stat_stream >> vms_pages >> rss_pages;
        long page_size = sysconf(_SC_PAGE_SIZE);
        vms = vms_pages * page_size / 1024 / 1024;
        rss = rss_pages * page_size / 1024 / 1024;
    }

    ScreenCapture::ScreenCapture(const std::string &display, int fps)
        : Filter("ScreenCapture"), m_display(display), m_fps(fps)
    {
    }

    ScreenCapture::~ScreenCapture()
    {
        stop();

        // 显式断开所有连接，防止在销毁过程中仍有信号进入导致的 pure virtual call
        if (m_videoSink)
        {
            disconnect(m_videoSink, nullptr, this, nullptr);
        }

        if (m_screenCapture)
            m_screenCapture->deleteLater();
        if (m_captureSession)
            m_captureSession->deleteLater();
        if (m_videoSink)
            m_videoSink->deleteLater();

        if (m_codecParams)
        {
            avcodec_parameters_free(&m_codecParams);
        }
    }

    bool ScreenCapture::initialize()
    {
        auto initFunc = [this]() -> bool
        {
            auto screens = QGuiApplication::screens();
            if (screens.isEmpty())
            {
                spdlog::error("No screens found for ScreenCapture");
                return false;
            }

            // Simple mapping: :0 or :0.0 -> screen 0, :1 -> screen 1
            int screenIdx = 0;
            if (m_display.find(":") != std::string::npos)
            {
                try
                {
                    size_t pos = m_display.find(".");
                    std::string idxStr = (pos == std::string::npos) ? m_display.substr(m_display.find(":") + 1) : m_display.substr(m_display.find(":") + 1, pos - m_display.find(":") - 1);
                    screenIdx = std::stoi(idxStr);
                }
                catch (...)
                {
                }
            }

            if (screenIdx < 0 || screenIdx >= screens.size())
                screenIdx = 0;
            QScreen *screen = screens.at(screenIdx);

            m_screenCapture = new QScreenCapture();
            m_screenCapture->setScreen(screen);

            m_captureSession = new QMediaCaptureSession();
            m_captureSession->setScreenCapture(m_screenCapture);

            m_videoSink = new QVideoSink();
            m_captureSession->setVideoOutput(m_videoSink);

            // Using DirectConnection because the background thread lacks an event loop.
            // handleFrame is thread-safe (uses mutex).
            connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &ScreenCapture::handleFrame, Qt::DirectConnection);

            connect(m_screenCapture, &QScreenCapture::activeChanged, this, [this](bool active)
                    { spdlog::info("[ScreenCapture] QScreenCapture active changed: {}", active); }, Qt::DirectConnection);

            m_codecParams = avcodec_parameters_alloc();
            m_codecParams->codec_type = AVMEDIA_TYPE_VIDEO;
            m_codecParams->codec_id = AV_CODEC_ID_RAWVIDEO;
            m_codecParams->format = AV_PIX_FMT_NV12;

            // 使用物理像素大小，防止 HighDPI 缩放导致分辨率不匹配
            QRect rect = screen->geometry();
            m_codecParams->width = rect.width() * screen->devicePixelRatio();
            m_codecParams->height = rect.height() * screen->devicePixelRatio();

            spdlog::info("ScreenCapture initialized on screen {}: logical {}x{}, physical {}x{}, ratio {}",
                         screenIdx, rect.width(), rect.height(), m_codecParams->width, m_codecParams->height, screen->devicePixelRatio());
            return true;
        };

        if (qGuiApp->thread() == QThread::currentThread())
        {
            m_initialized = initFunc();
        }
        else
        {
            QMetaObject::invokeMethod(qGuiApp, [this, &initFunc]()
                                      { m_initialized = initFunc(); }, Qt::BlockingQueuedConnection);
        }
        return m_initialized;
    }

    void ScreenCapture::start()
    {
        if (!m_initialized)
        {
            spdlog::error("[ScreenCapture] Cannot start: not initialized");
            return;
        }

        m_running = true;
        m_worker = std::thread(&ScreenCapture::workerThread, this);

        spdlog::info("[ScreenCapture] Activating QScreenCapture...");
        QMetaObject::invokeMethod(m_screenCapture, [this]
                                  { m_screenCapture->setActive(true); }, Qt::QueuedConnection);
    }

    void ScreenCapture::stop()
    {
        m_running = false;
        m_cv.notify_all();
        if (m_worker.joinable())
        {
            m_worker.join();
        }

        if (m_screenCapture)
        {
            QMetaObject::invokeMethod(m_screenCapture, [this]
                                      { m_screenCapture->setActive(false); }, Qt::QueuedConnection);
        }
    }

    AVCodecParameters *ScreenCapture::getCodecParameters() const
    {
        return m_codecParams;
    }

    void ScreenCapture::handleFrame(const QVideoFrame &frame)
    {
        static bool firstFrame = true;
        if (firstFrame)
        {
            firstFrame = false;
            int w = frame.width();
            int h = frame.height();
            spdlog::info("[ScreenCapture] First frame received: {}x{}, format={}", w, h, (int)frame.pixelFormat());

            // 关键：根据第一帧的真实分辨率更新 codecParams
            if (m_codecParams)
            {
                m_codecParams->width = w;
                m_codecParams->height = h;
                spdlog::info("[ScreenCapture] Updated codec parameters to match actual frame size: {}x{}", w, h);
            }
        }

        static int totalReceived = 0;

        if (!m_running)
            return;

        if (m_fps <= 0)
            m_fps = 30; // Safety fallback
        int64_t currentTime = QDateTime::currentMSecsSinceEpoch();
        int64_t interval = 1000 / m_fps;

        if (m_lastFrameTime > 0 && (currentTime - m_lastFrameTime) < (interval - 2))
        {
            return;
        }
        m_lastFrameTime = currentTime;

        // Perform immediate copy to release the QVideoFrame
        QVideoFrame f = frame;
        if (!f.map(QVideoFrame::ReadOnly))
            return;

        RawFrame raw;
        raw.width = f.width();
        raw.height = f.height();
        raw.bytesPerLine = f.bytesPerLine(0);
        raw.pixelFormat = f.pixelFormat();

        size_t size = raw.bytesPerLine * raw.height;
        raw.data.assign(f.bits(0), f.bits(0) + size);

        f.unmap();

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            int maxQueueSize = (m_latencyLevel == LatencyLevel::Standard) ? 3 : 1;
            while (m_frameQueue.size() >= maxQueueSize)
            {
                m_frameQueue.pop();
            }
            m_frameQueue.push(std::move(raw));
            m_cv.notify_one();
        }

        static int logCounter = 0;
        if (++logCounter % 300 == 0)
        {
            long vms, rss;
            get_memory_usage(vms, rss);
            spdlog::info("[ScreenCapture] RSS: {} MB, VMS: {} MB (Notice: 5GB+ VMS is normal on Wayland Portals)", rss, vms);
        }
    }

    void ScreenCapture::workerThread()
    {
        spdlog::info("[ScreenCapture] Worker thread started.");
        SwsContext *swsCtx = nullptr;
        int lastW = 0, lastH = 0;
        AVPixelFormat lastFmt = AV_PIX_FMT_NONE;

        while (m_running)
        {
            RawFrame raw;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_cv.wait(lock, [this]
                          { return !m_frameQueue.empty() || !m_running; });
                if (!m_running)
                    break;
                raw = std::move(m_frameQueue.front());
                m_frameQueue.pop();
            }

            // Determine input format from Qt to FFmpeg
            AVPixelFormat inFmt = AV_PIX_FMT_NONE;
            switch (raw.pixelFormat)
            {
            case QVideoFrameFormat::Format_ARGB8888:
            case QVideoFrameFormat::Format_XRGB8888:
                // 在很多 Linux Wayland 环境下，虽然叫 ARGB，但字节序实际是 RGBA
                // 如果发现画面蓝变黄，尝试在 BGRA 和 RGBA 之间切换
                inFmt = AV_PIX_FMT_RGBA;
                break;
            case QVideoFrameFormat::Format_BGRA8888:
            case QVideoFrameFormat::Format_BGRX8888:
                inFmt = AV_PIX_FMT_BGRA;
                break;
            case QVideoFrameFormat::Format_ABGR8888:
            case QVideoFrameFormat::Format_XBGR8888:
                inFmt = AV_PIX_FMT_RGBA;
                break;
            default:
                inFmt = AV_PIX_FMT_RGBA;
                break;
            }

            int w = raw.width;
            int h = raw.height;

            if (!swsCtx || w != lastW || h != lastH || inFmt != lastFmt)
            {
                if (swsCtx)
                    sws_freeContext(swsCtx);

                int flags = (m_latencyLevel == LatencyLevel::Standard) ? SWS_BILINEAR : SWS_FAST_BILINEAR;
                swsCtx = sws_getContext(w, h, inFmt,
                                        w, h, AV_PIX_FMT_NV12,
                                        flags, nullptr, nullptr, nullptr);

                // 设置颜色空间细节，处理 Full Range RGB -> Limited Range YUV 的转换
                // 强制指定：源是 Full Range (1), 目标是 Limited Range (0)
                // 使用 BT.709 转换系数
                const int *coeffs = sws_getCoefficients(SWS_CS_ITU709);
                sws_setColorspaceDetails(swsCtx, coeffs, 1, coeffs, 0,
                                         0, 1 << 16, 1 << 16);

                lastW = w;
                lastH = h;
                lastFmt = inFmt;
            }

            auto frameWrapper = std::make_shared<AVFrameWrapper>();
            AVFrame *avFrame = frameWrapper->get();
            avFrame->width = w;
            avFrame->height = h;
            avFrame->format = AV_PIX_FMT_NV12;
            avFrame->pts = m_frameCount++;

            if (av_frame_get_buffer(avFrame, 32) < 0)
            {
                continue;
            }

            const uint8_t *srcData[4] = {raw.data.data(), nullptr, nullptr, nullptr};
            int srcLinesize[4] = {raw.bytesPerLine, 0, 0, 0};

            sws_scale(swsCtx, srcData, srcLinesize, 0, h,
                      avFrame->data, avFrame->linesize);

            if (m_next)
            {
                static int frameLog = 0;
                if (++frameLog % 60 == 0)
                {
                    spdlog::info("[ScreenCapture] Sent frame {} to next filter ({}x{})", avFrame->pts, w, h);
                }
                m_next->process(frameWrapper);
            }
        }

        if (swsCtx)
            sws_freeContext(swsCtx);
    }

} // namespace pb
