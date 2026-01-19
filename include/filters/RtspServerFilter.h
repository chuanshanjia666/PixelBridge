#ifndef RTSPSERVERFILTER_H
#define RTSPSERVERFILTER_H

#include "core/Filter.h"
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace pb
{

    class RtspServerFilter : public Filter
    {
    public:
        RtspServerFilter(int port = 8554, const std::string &streamName = "live", const std::string &address = "");
        ~RtspServerFilter();

        bool initialize(AVCodecContext *encoderCtx);
        bool initialize() override { return false; } // Use the parameterized version

        void process(DataPacket::Ptr packet) override;
        void stop() override;

    private:
        void serverLoop();

        int m_port;
        std::string m_streamName;
        std::string m_address;
        std::thread m_serverThread;
        std::atomic<bool> m_running{false};

        UsageEnvironment *m_env = nullptr;
        TaskScheduler *m_scheduler = nullptr;
        RTSPServer *m_rtspServer = nullptr;

        // Queue for passing packets to live555
        std::queue<DataPacket::Ptr> m_packetQueue;
        std::mutex m_queueMutex;
        std::condition_variable m_queueCv;

        AVCodecContext *m_encoderCtx = nullptr;
        EventLoopWatchVariable m_watchVariable{0};
    };

} // namespace pb

#endif // RTSPSERVERFILTER_H
