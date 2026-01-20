#include "core/Logger.h"

Logger::Logger(QObject *parent) : QAbstractListModel(parent) {}

int Logger::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_logs.size());
}

QVariant Logger::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    std::lock_guard<std::mutex> lock(m_mutex);
    if (index.row() < 0 || index.row() >= static_cast<int>(m_logs.size()))
        return QVariant();

    const auto &entry = m_logs[index.row()];
    if (role == TimestampRole)
        return entry.timestamp;
    if (role == LevelRole)
        return entry.level;
    if (role == MessageRole)
        return entry.message;

    return QVariant();
}

QHash<int, QByteArray> Logger::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TimestampRole] = "timestamp";
    roles[LevelRole] = "level";
    roles[MessageRole] = "message";
    return roles;
}

void Logger::addLog(const QString &level, const QString &message)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    entry.level = level;
    entry.message = message;

    // Use meta-object system to ensure thread safety when updating the model for QML
    QMetaObject::invokeMethod(this, [this, entry]()
                              {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_logs.push_back(entry);
            // Optional: limit log size
            if (m_logs.size() > 1000) {
                // Handle truncation if needed
            }
        }
        endInsertRows(); }, Qt::QueuedConnection);
}

void Logger::clear()
{
    beginResetModel();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.clear();
    }
    endResetModel();
}
