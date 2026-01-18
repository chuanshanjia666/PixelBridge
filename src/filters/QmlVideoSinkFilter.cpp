#include "filters/QmlVideoSinkFilter.h"
#include <spdlog/spdlog.h>

namespace pb
{

    QmlVideoSinkFilter::QmlVideoSinkFilter() : Filter("QmlVideoSinkFilter") {}

    QmlVideoSinkFilter::~QmlVideoSinkFilter()
    {
        if (m_swsContext)
        {
            sws_freeContext(m_swsContext);
        }
    }

    void QmlVideoSinkFilter::setVideoSink(QVideoSink *sink)
    {
        if (m_videoSink != sink)
        {
            m_videoSink = sink;
            emit videoSinkChanged();
        }
    }

    bool QmlVideoSinkFilter::initialize()
    {
        return true;
    }

    void QmlVideoSinkFilter::process(DataPacket::Ptr packet)
    {
        if (!m_videoSink || packet->type() != PacketType::AV_FRAME)
        {
            return;
        }

        static int qmlLog = 0;
        if (++qmlLog % 60 == 0)
        {
            spdlog::info("[QmlVideoSinkFilter] Rendering frame for QML preview");
        }

        auto frameWrapper = std::static_pointer_cast<AVFrameWrapper>(packet);
        AVFrame *frame = frameWrapper->get();

        if (!m_swsContext || m_width != frame->width || m_height != frame->height || m_format != frame->format)
        {
            if (m_swsContext)
            {
                sws_freeContext(m_swsContext);
            }
            m_width = frame->width;
            m_height = frame->height;
            m_format = frame->format;
            m_swsContext = sws_getContext(m_width, m_height, (AVPixelFormat)m_format,
                                          m_width, m_height, AV_PIX_FMT_RGBA, // QML 预览通常更喜欢 RGBA 对应 Format_RGBA8888 或 Format_ARGB8888
                                          SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        }

        QVideoFrameFormat qFormat(QSize(m_width, m_height), QVideoFrameFormat::Format_RGBA8888);
        QVideoFrame qFrame(qFormat);
        if (qFrame.map(QVideoFrame::WriteOnly))
        {
            uint8_t *dst[] = {qFrame.bits(0)};
            int dstLinesize[] = {(int)qFrame.bytesPerLine(0)};
            sws_scale(m_swsContext, frame->data, frame->linesize, 0, m_height, dst, dstLinesize);
            qFrame.unmap();
            m_videoSink->setVideoFrame(qFrame);
        }
    }

    void QmlVideoSinkFilter::stop() {}

} // namespace pb
