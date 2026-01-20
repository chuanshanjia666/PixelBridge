#ifndef LOGGER_H
#define LOGGER_H

#include <QAbstractListModel>
#include <QStringList>
#include <QDateTime>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

struct LogEntry
{
    QString timestamp;
    QString level;
    QString message;
};

class Logger : public QAbstractListModel
{
    Q_OBJECT

public:
    enum LogRoles
    {
        TimestampRole = Qt::UserRole + 1,
        LevelRole,
        MessageRole
    };

    static Logger &instance()
    {
        static Logger inst;
        return inst;
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addLog(const QString &level, const QString &message);

    Q_INVOKABLE void clear();

private:
    Logger(QObject *parent = nullptr);
    std::vector<LogEntry> m_logs;
    mutable std::mutex m_mutex;
};

template <typename Mutex>
class QmlLogSink : public spdlog::sinks::base_sink<Mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg &msg) override
    {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

        QString levelStr;
        switch (msg.level)
        {
        case spdlog::level::debug:
            levelStr = "DEBUG";
            break;
        case spdlog::level::info:
            levelStr = "INFO";
            break;
        case spdlog::level::warn:
            levelStr = "WARN";
            break;
        case spdlog::level::err:
            levelStr = "ERROR";
            break;
        case spdlog::level::critical:
            levelStr = "CRITICAL";
            break;
        default:
            levelStr = "TRACE";
            break;
        }

        QString message = QString::fromStdString(std::string(msg.payload.data(), msg.payload.size()));
        Logger::instance().addLog(levelStr, message);
    }

    void flush_() override {}
};

using QmlLogSinkMt = QmlLogSink<std::mutex>;

#endif // LOGGER_H
