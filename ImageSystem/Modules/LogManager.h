#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include "CommonFunc.h"

class LogManager : public QObject
{
    Q_OBJECT
    SINGLETON_CLASS(LogManager)

    Q_PROPERTY(QString logText READ logText NOTIFY logChanged)

public:
    QString logText() const { return m_logText; }

    Q_INVOKABLE void appendLog(const QString& text);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void appendTimestamped(const QString& category, const QString& text);

signals:
    void logChanged();

private:
    QString m_logText;
    QTimer* m_throttleTimer = nullptr;
    bool m_dirty = false;

    void scheduleUpdate();
};
