#include "LogManager.h"
#include <QTimer>

LogManager::LogManager(QObject *parent)
    : QObject(parent)
{
}

void LogManager::appendLog(const QString& text)
{
    if (text.isEmpty())
        return;
    m_logText.append(text);
    scheduleUpdate();
}

void LogManager::clear()
{
    m_logText.clear();
    m_dirty = false;
    emit logChanged();
}

void LogManager::appendTimestamped(const QString& category, const QString& text)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1][%2] %3\n").arg(ts, category, text.trimmed());
    appendLog(line);
}

void LogManager::scheduleUpdate()
{
    if (!m_throttleTimer) {
        m_throttleTimer = new QTimer(this);
        m_throttleTimer->setSingleShot(true);
        m_throttleTimer->setInterval(200);
        connect(m_throttleTimer, &QTimer::timeout, this, [this]() {
            if (m_dirty) {
                m_dirty = false;
                emit logChanged();
            }
        });
    }

    m_dirty = true;
    if (!m_throttleTimer->isActive()) {
        m_throttleTimer->start();
    }
}
