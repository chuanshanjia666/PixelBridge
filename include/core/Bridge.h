#ifndef BRIDGE_H
#define BRIDGE_H

#include <QObject>
#include <QString>
#include <QVideoSink>
#include <memory>
#include <vector>
#include <mutex>
#include "filters/QmlVideoSinkFilter.h"
#include "core/Filter.h"

namespace pb
{
    class TeeFilter;
}

class Bridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVideoSink *videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(QStringList hwTypes READ hwTypes CONSTANT)

public:
    explicit Bridge(QObject *parent = nullptr);
    virtual ~Bridge();

    QVideoSink *videoSink() const;
    void setVideoSink(QVideoSink *sink);
    QStringList hwTypes() const;

    Q_INVOKABLE void startPlay(const QString &url, const QString &hwType, int latencyLevel = 1);
    Q_INVOKABLE void startServe(const QString &source, int port, const QString &name, const QString &encoder, const QString &hw, int fps = 30, int latencyLevel = 1);
    Q_INVOKABLE void startPush(const QString &input, const QString &output, const QString &encoder, const QString &hw, int fps = 30, int latencyLevel = 1);
    Q_INVOKABLE void stopAll();
    Q_INVOKABLE QString urlToPath(const QUrl &url);

signals:
    void videoSinkChanged();

private:
    pb::QmlVideoSinkFilter *m_qmlSink;
    std::mutex m_chainMutex;
    std::vector<std::vector<std::shared_ptr<pb::Filter>>> m_chains;
};

#endif // BRIDGE_H
