#ifndef DEMUXER_H
#define DEMUXER_H

#include "core/Filter.h"
#include <string>
#include <thread>
#include <atomic>

namespace pb
{

    class Demuxer : public Filter
    {
    public:
        Demuxer(const std::string &url);
        ~Demuxer();

        bool initialize() override;
        void process(DataPacket::Ptr packet) override; // Normally Demuxer is a source, it doesn't process incoming packets
        void stop() override;
        void start() override;

        AVCodecParameters *getVideoCodecParameters() const;

    private:
        void run();

        std::string m_url;
        AVFormatContext *m_formatCtx = nullptr;
        std::thread m_thread;
        std::atomic<bool> m_running{false};
        int m_videoStreamIndex = -1;
    };

} // namespace pb

#endif // DEMUXER_H
