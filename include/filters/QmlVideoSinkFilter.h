#ifndef QMLVIDEOSINKFILTER_H
#define QMLVIDEOSINKFILTER_H

#include <QObject>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <mutex>
#include "core/Filter.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
}

namespace pb
{

    class QmlVideoSinkFilter : public QObject, public Filter
    {
        Q_OBJECT
        Q_PROPERTY(QVideoSink *videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)

    public:
        QmlVideoSinkFilter();
        ~QmlVideoSinkFilter();

        QVideoSink *videoSink() const { return m_videoSink; }
        void setVideoSink(QVideoSink *sink);

        bool initialize() override;
        void process(DataPacket::Ptr packet) override;
        void stop() override;

    signals:
        void videoSinkChanged();

    private:
        QVideoSink *m_videoSink = nullptr;
        SwsContext *m_swsContext = nullptr;
        int m_width = 0;
        int m_height = 0;
        int m_format = -1;
    };

} // namespace pb

#endif // QMLVIDEOSINKFILTER_H
