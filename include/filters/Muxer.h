#ifndef MUXER_H
#define MUXER_H

#include "core/Filter.h"
#include <string>

namespace pb
{

    class Muxer : public Filter
    {
    public:
        Muxer(const std::string &url);
        ~Muxer();

        bool initialize(AVCodecContext *encoderCtx);
        bool initialize() override { return false; } // Use the parameterized version

        void process(DataPacket::Ptr packet) override;
        void stop() override;

    private:
        std::string m_url;
        AVFormatContext *m_formatCtx = nullptr;
        AVStream *m_outStream = nullptr;
        AVRational m_srcTimeBase = {1, 30};
        bool m_headerWritten = false;
        bool m_trailerWritten = false;
    };

} // namespace pb

#endif // MUXER_H
