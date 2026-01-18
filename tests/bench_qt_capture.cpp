#include <QGuiApplication>
#include <QScreenCapture>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QTimer>
#include <iostream>
#include <chrono>
#include <fstream>
#include <unistd.h>

// 获取内存统计：真正的 RSS (物理内存) 和 VMS (虚拟内存)
void get_mem(long &vms, long &rss)
{
    std::ifstream stat_stream("/proc/self/statm", std::ios_base::in);
    long vms_pages, rss_pages;
    stat_stream >> vms_pages >> rss_pages;
    long page_size = sysconf(_SC_PAGE_SIZE);
    vms = vms_pages * page_size / 1024 / 1024;
    rss = rss_pages * page_size / 1024 / 1024;
}

class CaptureBench : public QObject
{
    Q_OBJECT
public:
    CaptureBench()
    {
        m_screenCapture = new QScreenCapture(this);
        m_session = new QMediaCaptureSession(this);
        m_sink = new QVideoSink(this);

        m_session->setScreenCapture(m_screenCapture);
        m_session->setVideoOutput(m_sink);

        // 统计逻辑
        connect(m_sink, &QVideoSink::videoFrameChanged, this, &CaptureBench::onFrameChanged);

        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &CaptureBench::printStats);
        m_timer->start(1000);

        m_startTime = std::chrono::steady_clock::now();
        m_screenCapture->setActive(true);

        long vms, rss;
        get_mem(vms, rss);
        std::cout << "Capture started. Monitoring for 30 seconds..." << std::endl;
        std::cout << "Initial - RSS: " << rss << " MB, VMS: " << vms << " MB" << std::endl;
    }

private slots:
    void onFrameChanged(const QVideoFrame &frame)
    {
        m_frameCount++;

        // 记录两帧之间的时间间隔，看看是不是源端在抖动
        auto now = std::chrono::steady_clock::now();
        if (m_lastFrameTime.time_since_epoch().count() > 0)
        {
            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameTime).count();
            m_maxDelta = std::max(m_maxDelta, (int)delta);
        }
        m_lastFrameTime = now;

        // 测试 Mapping 性能
        auto start = std::chrono::steady_clock::now();
        QVideoFrame f = frame; // 引用计数
        if (f.map(QVideoFrame::ReadOnly))
        {
            auto end = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            m_totalMapTime += ms;
            if (ms > 5)
                m_slowMaps++;

            // 关键：在这里观察格式
            if (m_formatName == QVideoFrameFormat::Format_Invalid)
            {
                m_formatName = f.pixelFormat();
            }

            f.unmap();
        }
    }

    void printStats()
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();

        long vms, rss;
        get_mem(vms, rss);
        double avgMap = m_frameCount > 0 ? (double)m_totalMapTime / m_frameCount : 0;

        std::cout << "[" << elapsed << "s] FPS: " << m_frameCount
                  << " | RSS: " << rss << " MB (VMS: " << vms << " MB)"
                  << " | AvgMap: " << avgMap << "ms"
                  << " | MaxJitter: " << m_maxDelta << "ms"
                  << " | Format: " << m_formatName << std::endl;

        m_frameCount = 0;
        m_totalMapTime = 0;
        m_slowMaps = 0;
        m_maxDelta = 0;

        if (elapsed >= 30)
            qGuiApp->quit();
    }

private:
    QScreenCapture *m_screenCapture;
    QMediaCaptureSession *m_session;
    QVideoSink *m_sink;
    QTimer *m_timer;

    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastFrameTime;
    int m_frameCount = 0;
    long long m_totalMapTime = 0;
    int m_slowMaps = 0;
    int m_maxDelta = 0;
    QVideoFrameFormat::PixelFormat m_formatName = QVideoFrameFormat::Format_Invalid;
    int m_format = 0; // dummy
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    CaptureBench bench;
    return app.exec();
}

#include "bench_qt_capture.moc"
