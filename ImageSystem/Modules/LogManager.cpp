#include "LogManager.h"
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif

LogManager::LogManager(QObject *parent)
    : QObject(parent)
{
}

void LogManager::appendLog(const QString& text)
{
    if (text.isEmpty())
        return;
    m_logText.append(text);
    appendToDisk(text);
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

QString LogManager::resolveSessionLogFilePath()
{
    if (!m_currentLogFilePath.isEmpty())
        return m_currentLogFilePath;

    const QString logDirPath = QStringLiteral("D:/AetherDesk/log");
    QDir dir;
    if (!dir.mkpath(logDirPath))
        return {};

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString fileName = QStringLiteral("aetherdesk_%1.txt").arg(stamp);
    m_currentLogFilePath = QDir(logDirPath).filePath(fileName);
    return m_currentLogFilePath;
}

void LogManager::appendToDisk(const QString& text)
{
    const QString filePath = resolveSessionLogFilePath();
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out << text;
    file.close();
}
