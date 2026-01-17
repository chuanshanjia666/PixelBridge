#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include "core/Filter.h"
#include <thread>
#include <atomic>

extern "C"
{
#include <libavdevice/avdevice.h>
}

namespace pb
{

    class ScreenCapture : public Filter
    {
    public:
        ScreenCapture(const std::string &display = ":0.0");
        ~ScreenCapture();

        bool initialize() override;
        void process(DataPacket::Ptr packet) override;
        void stop() override;
        void start() override;

        AVCodecParameters *getCodecParameters() const;

    private:
        void run();

        std::string m_display;
        AVFormatContext *m_formatCtx = nullptr;
        std::thread m_thread;
        std::atomic<bool> m_running{false};
        int m_videoStreamIndex = -1;
    };

} // namespace pb

#endif // SCREENCAPTURE_H
