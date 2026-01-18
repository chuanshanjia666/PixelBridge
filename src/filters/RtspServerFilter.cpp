#include "filters/RtspServerFilter.h"
#include <spdlog/spdlog.h>
#include <OnDemandServerMediaSubsession.hh>
#include <H264VideoRTPSink.hh>
#include <H264VideoStreamFramer.hh>
#include <Base64.hh>

namespace pb
{
    class PacketSource : public FramedSource
    {
    public:
        static PacketSource *createNew(UsageEnvironment &env,
                                       std::queue<DataPacket::Ptr> &queue,
                                       std::mutex &mutex,
                                       std::condition_variable &cv)
        {
            return new PacketSource(env, queue, mutex, cv);
        }

    protected:
        PacketSource(UsageEnvironment &env,
                     std::queue<DataPacket::Ptr> &queue,
                     std::mutex &mutex,
                     std::condition_variable &cv)
            : FramedSource(env), m_queue(queue), m_mutex(mutex), m_cv(cv) {}

        void doGetNextFrame() override
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_queue.empty())
            {
                nextTask() = envir().taskScheduler().scheduleDelayedTask(1000, (TaskFunc *)staticDoGetNextFrame, this);
                return;
            }

            auto packet = m_queue.front();
            auto pktWrapper = std::static_pointer_cast<AVPacketWrapper>(packet);
            AVPacket *pkt = pktWrapper->get();

            m_queue.pop();
            lock.unlock();

            if (pkt->size > fMaxSize)
            {
                fFrameSize = fMaxSize;
                fNumTruncatedBytes = pkt->size - fMaxSize;
            }
            else
            {
                fFrameSize = pkt->size;
                fNumTruncatedBytes = 0;
            }

            memcpy(fTo, pkt->data, fFrameSize);
            gettimeofday(&fPresentationTime, NULL);
            FramedSource::afterGetting(this);
        }

        static void staticDoGetNextFrame(void *clientData)
        {
            ((PacketSource *)clientData)->doGetNextFrame();
        }

    private:
        std::queue<DataPacket::Ptr> &m_queue;
        std::mutex &m_mutex;
        std::condition_variable &m_cv;
    };

    class LiveH264Subsession : public OnDemandServerMediaSubsession
    {
    public:
        static LiveH264Subsession *createNew(UsageEnvironment &env,
                                             std::queue<DataPacket::Ptr> &queue,
                                             std::mutex &mutex,
                                             std::condition_variable &cv,
                                             AVCodecContext *encoderCtx)
        {
            return new LiveH264Subsession(env, queue, mutex, cv, encoderCtx);
        }

    protected:
        LiveH264Subsession(UsageEnvironment &env,
                           std::queue<DataPacket::Ptr> &queue,
                           std::mutex &mutex,
                           std::condition_variable &cv,
                           AVCodecContext *encoderCtx)
            : OnDemandServerMediaSubsession(env, True), m_queue(queue), m_mutex(mutex), m_cv(cv), m_encoderCtx(encoderCtx) {}

        FramedSource *createNewStreamSource(unsigned /*clientSessionId*/, unsigned &estBitrate) override
        {
            estBitrate = 4000; // kbps
            auto source = PacketSource::createNew(envir(), m_queue, m_mutex, m_cv);
            return H264VideoStreamFramer::createNew(envir(), source);
        }

        RTPSink *createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource * /*inputSource*/) override
        {
            return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
        }

    private:
        std::queue<DataPacket::Ptr> &m_queue;
        std::mutex &m_mutex;
        std::condition_variable &m_cv;
        AVCodecContext *m_encoderCtx;
    };

    RtspServerFilter::RtspServerFilter(int port, const std::string &streamName)
        : Filter("RtspServerFilter"), m_port(port), m_streamName(streamName)
    {
    }

    RtspServerFilter::~RtspServerFilter()
    {
        stop();

        // Final cleanup of Live555 objects
        if (m_rtspServer)
        {
            Medium::close(m_rtspServer);
        }
        if (m_env)
        {
            m_env->reclaim();
        }
        if (m_scheduler)
        {
            delete m_scheduler;
        }
    }

    bool RtspServerFilter::initialize(AVCodecContext *encoderCtx)
    {
        m_encoderCtx = encoderCtx;

        // 降低 live555 默认缓冲区大小，避免 5GB 级别的虚拟内存分配
        // 1080P H.264 关键帧通常在 500KB-1MB 左右，2MB 足够安全
        if (OutPacketBuffer::maxSize < 2000000)
        {
            OutPacketBuffer::maxSize = 2000000;
        }

        m_scheduler = BasicTaskScheduler::createNew();
        m_env = BasicUsageEnvironment::createNew(*m_scheduler);

        m_rtspServer = RTSPServer::createNew(*m_env, m_port);
        if (!m_rtspServer)
        {
            spdlog::error("Failed to create RTSP server on port {}", m_port);
            return false;
        }

        ServerMediaSession *sms = ServerMediaSession::createNew(*m_env, m_streamName.c_str(), "PixelBridge Live Stream", "H.264 streaming from PixelBridge");
        sms->addSubsession(LiveH264Subsession::createNew(*m_env, m_packetQueue, m_queueMutex, m_queueCv, m_encoderCtx));
        m_rtspServer->addServerMediaSession(sms);

        char *url = m_rtspServer->rtspURL(sms);
        spdlog::info("RTSP Server started at {}", url);
        delete[] url;

        m_running = true;
        m_serverThread = std::thread(&RtspServerFilter::serverLoop, this);

        return true;
    }

    void RtspServerFilter::process(DataPacket::Ptr packet)
    {
        if (packet->type() != PacketType::AV_PACKET)
            return;

        static int packetLog = 0;
        if (++packetLog % 60 == 0)
        {
            auto pkt = std::static_pointer_cast<AVPacketWrapper>(packet)->get();
            spdlog::info("[RtspServerFilter] Received packet pts={}, size={}", pkt->pts, pkt->size);
        }

        std::lock_guard<std::mutex> lock(m_queueMutex);
        // 严格限制队列深度到 10 帧，约 0.3s 的缓冲。
        // 超出时直接丢掉最旧的帧，确保内存不爆炸。
        while (m_packetQueue.size() >= 10)
        {
            m_packetQueue.pop();
        }
        m_packetQueue.push(packet);
        m_queueCv.notify_one();
    }

    void RtspServerFilter::stop()
    {
        m_running = false;
        m_watchVariable = 1;
        if (m_serverThread.joinable())
        {
            m_serverThread.join();
        }
    }

    void RtspServerFilter::serverLoop()
    {
        if (m_env)
        {
            m_env->taskScheduler().doEventLoop(&m_watchVariable);
        }
    }

} // namespace pb
