#pragma once

#include "CommonFunc.h"
#include <QStringList>
#include <QRegularExpression>
#include <QtConcurrent>

class PythonConsoleManager : public QObject
{
    Q_OBJECT
    SINGLETON_CLASS(PythonConsoleManager)
    QUICK_PROPERTY(bool, isExecuting)

public:
    Q_INVOKABLE void executeCommand(const QString& command);
    Q_INVOKABLE void clearOutput();
    Q_INVOKABLE QString getHistory(int index) const;
    Q_INVOKABLE int historySize() const;

signals:
    void outputAppended(const QString& text);

private:
    void doExecute(const QString& command);
    QStringList m_history;
};
