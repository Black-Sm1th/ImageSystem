#include "KnowledgeChatManager.h"
#include "Modules/ApiManager.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QVariantMap>
#include <QXmlStreamReader>
#include <QClipboard>
#include <QGuiApplication>
#include <QtConcurrent/QtConcurrent>
#include <QTimer>
#include <QRegularExpression>

namespace {
    // 配置常量
    constexpr int DEFAULT_MAX_FILE_COUNT = 3;                    ///< 默认最大文件数量
    constexpr qint64 DEFAULT_MAX_FILE_SIZE = 10 * 1024 * 1024;   ///< 默认最大文件大小 (10MB)
    constexpr int THREAD_WAIT_TIMEOUT = 3000;                    ///< 线程等待超时时间 (毫秒)
    constexpr int POWERSHELL_TIMEOUT = 30000;                    ///< PowerShell执行超时时间 (毫秒)
    constexpr int PROGRESS_ANIMATION_DELAY = 50;                 ///< 进度动画延迟 (毫秒)

    // 文件读取相关常量
    const QStringList SUPPORTED_TEXT_FORMATS = { "txt", "doc", "docx" };
    const QStringList SUPPORTED_IMAGE_FORMATS = { "jpg", "jpeg", "png", "bmp", "gif" };
}

KnowledgeChatManager::KnowledgeChatManager(QObject* parent)
    : QObject(parent)
    , m_isSending(false)
    , m_isThinking(false)
    , m_lastUserMessage("")
    , m_updateTimer(new QTimer(this))
{
    // 连接API管理器的信号
    auto* apiManager = GET_SINGLETON(ApiManager);
    connect(apiManager, &ApiManager::streamChatResponse,
        this, &KnowledgeChatManager::onStreamChatResponse);
    connect(apiManager, &ApiManager::streamChatFinished,
        this, &KnowledgeChatManager::onStreamChatFinished);
    connect(apiManager, &ApiManager::streamKnowledgeChatResponse,
        this, &KnowledgeChatManager::onStreamKnowledgeChatResponse);
    connect(apiManager, &ApiManager::streamKnowledgeChatFinished,
        this, &KnowledgeChatManager::onStreamKnowledgeChatFinished);

    // 初始化聊天会话ID
    m_currentChatId = CommonFunc::generateNumericUUID();
    apiManager->loginUser("ImageSystem", "12345678");
    // 配置UI更新定时器
    m_updateTimer->setInterval(30);  // 每30ms批量更新一次UI,平衡流畅度和性能
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, &QTimer::timeout, this, &KnowledgeChatManager::flushPendingUpdates);
}

void KnowledgeChatManager::sendMessage(const QString& message)
{
    if (message.trimmed().isEmpty() || m_isSending) {
        return;
    }
    QString trimmedMessage = message.trimmed();
    QString fullMessage = trimmedMessage;

    // 保存消息用于重新生成
    setlastUserMessage(fullMessage);

    // 添加用户消息到界面（显示原始消息）
    addUserMessage(trimmedMessage);

    // 设置发送状态
    setisSending(true);
    setisThinking(true);
    m_currentAiMessage.clear();

    // 显示思考状态
    addThinkingMessage();


    auto* apiManager = GET_SINGLETON(ApiManager);
    QString language = "zh"; // 默认中文，可以根据需要调整
    const QString userId = "2017074164166459394";

    apiManager->streamKnowledgeChat(fullMessage, userId, language, {}, m_currentChatId);
}


void KnowledgeChatManager::regenerateLastResponse()
{
    if (getlastUserMessage().isEmpty() || m_isSending) {
        return;
    }

    // 移除最后一条AI消息
    QVariantList currentMessages = getmessages();
    if (!currentMessages.isEmpty()) {
        QVariantMap lastMessage = currentMessages.last().toMap();
        if (lastMessage["type"].toString() == "ai") {
            currentMessages.removeLast();
            setmessages(currentMessages);
        }
    }

    // 重新发送最后一条消息
    QString lastMessage = getlastUserMessage();

    // 设置状态
    setisSending(true);
    setisThinking(true);
    m_currentAiMessage.clear();

    addThinkingMessage();

    // 发送API请求
    const QString userId = "2017074164166459394";

    auto* apiManager = GET_SINGLETON(ApiManager);

    QString language = "zh"; // 默认中文，可以根据需要调整

    apiManager->streamKnowledgeChat(lastMessage, userId, language, {}, m_currentChatId);
}

void KnowledgeChatManager::endAnalysis(bool clearfile)
{
    // 中断当前聊天
    GET_SINGLETON(ApiManager)->abortStreamChatByChatId(m_currentChatId);

    // 停止定时器并立即刷新待更新内容
    m_updateTimer->stop();
    flushPendingUpdates();

    // 重置状态
    m_currentAiMessage.clear();
    m_pendingUpdateBuffer.clear();
    setisSending(false);
    setisThinking(false);

    // 替换思考消息为中断消息
    QVariantList currentMessages = getmessages();
    if (!currentMessages.isEmpty() && currentMessages.last().toMap()["type"] == "thinking") {
        currentMessages.removeLast();

        QVariantMap interruptMessage;
        interruptMessage["type"] = "interrupt";
        interruptMessage["content"] = QStringLiteral("消息已中断！");
        interruptMessage["timestamp"] = QDateTime::currentDateTime().toString("hh:mm");
        currentMessages.append(interruptMessage);

        setmessages(currentMessages);
    }
}

void KnowledgeChatManager::addUserMessage(const QString& message)
{
    QVariantMap userMessage;
    userMessage["type"] = "user";
    userMessage["content"] = message;
    userMessage["timestamp"] = QDateTime::currentDateTime().toString("hh:mm");

    QVariantList currentMessages = getmessages();
    currentMessages.append(userMessage);
    setmessages(currentMessages);
}

void KnowledgeChatManager::addAiMessage(const QString& message)
{
    QVariantMap aiMessage;
    aiMessage["type"] = "ai";
    aiMessage["content"] = message;
    aiMessage["timestamp"] = QDateTime::currentDateTime().toString("hh:mm");

    QVariantList currentMessages = getmessages();
    currentMessages.append(aiMessage);
    setmessages(currentMessages);
}

void KnowledgeChatManager::addThinkingMessage()
{
    QVariantMap thinkingMessage;
    thinkingMessage["type"] = "thinking";
    thinkingMessage["content"] = QStringLiteral("思考中");
    thinkingMessage["timestamp"] = QDateTime::currentDateTime().toString("hh:mm");

    QVariantList currentMessages = getmessages();
    currentMessages.append(thinkingMessage);
    setmessages(currentMessages);
}

void KnowledgeChatManager::removeThinkingMessage()
{
    QVariantList currentMessages = getmessages();

    // 从后往前查找并移除思考中消息
    for (int i = currentMessages.size() - 1; i >= 0; i--) {
        QVariantMap message = currentMessages[i].toMap();
        if (message["type"].toString() == "thinking") {
            currentMessages.removeAt(i);
            setmessages(currentMessages);
            break;
        }
    }
}

void KnowledgeChatManager::updateLastAiMessage(const QString& additionalText)
{
    QVariantList currentMessages = getmessages();
    if (currentMessages.isEmpty()) {
        return;
    }

    QVariantMap lastMessage = currentMessages.last().toMap();
    if (lastMessage["type"].toString() == "ai") {
        // 追加内容
        QString currentContent = lastMessage["content"].toString();
        lastMessage["content"] = currentContent + additionalText;

        // 更新消息列表
        currentMessages.removeLast();
        currentMessages.append(lastMessage);
        setmessages(currentMessages);
    }
}

void KnowledgeChatManager::onStreamChatResponse(const QString& data, const QString& chatId)
{
    // 验证是否为当前会话
    if (chatId != m_currentChatId) {
        return;
    }

    if (m_currentAiMessage.isEmpty()) {
        // 第一次接收响应：移除思考状态
        setisThinking(false);
        removeThinkingMessage();
        addAiMessage(data);
        m_currentAiMessage = data;
    }
    else {
        // 追加响应内容
        m_currentAiMessage += data;
        updateLastAiMessage(data);
    }
}

void KnowledgeChatManager::onStreamChatFinished(bool success, const QString& message, const QString& chatId)
{
    // 验证是否为当前会话
    if (chatId != m_currentChatId) {
        return;
    }

    qDebug() << "[KnowledgeChatManager] Chat finished, success:" << success;

    // 重置状态
    setisSending(false);
    setisThinking(false);

    if (!success) {
        // 处理错误情况
        removeThinkingMessage();
        addAiMessage(QStringLiteral("抱歉，发生了错误：%1").arg(message));
    }

    // 处理空响应
    if (m_currentAiMessage.isEmpty()) {
        removeThinkingMessage();
        addAiMessage(QStringLiteral("抱歉，我无法回复您的消息。"));
    }

    m_currentAiMessage.clear();
}

void KnowledgeChatManager::copyToClipboard(const QString& content)
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(content);
}

void KnowledgeChatManager::onStreamKnowledgeChatResponse(const QString& data, const QString& chatId)
{
    // 验证是否为当前会话
    if (chatId != m_currentChatId) {
        return;
    }

    if (m_currentAiMessage.isEmpty()) {
        // 第一次接收响应：移除思考状态
        setisThinking(false);
        removeThinkingMessage();
        addAiMessage(data);
        m_currentAiMessage = data;
        m_pendingUpdateBuffer.clear();  // 清空缓冲区
    }
    else {
        // 将新数据放入缓冲区
        m_pendingUpdateBuffer += data;
        m_currentAiMessage += data;
        
        // 启动或重启定时器(单次触发模式会自动重置)
        if (!m_updateTimer->isActive()) {
            m_updateTimer->start();
        }
    }
}

void KnowledgeChatManager::onStreamKnowledgeChatFinished(bool success, const QString& message, const QString& chatId)
{
    // 验证是否为当前会话
    if (chatId != m_currentChatId) {
        return;
    }

    qDebug() << "[KnowledgeChatManager] Knowledge chat finished, success:" << success;

    // 停止定时器并立即刷新所有待更新的内容
    m_updateTimer->stop();
    flushPendingUpdates();

    // 重置状态
    setisSending(false);
    setisThinking(false);

    if (!success) {
        // 处理错误情况
        removeThinkingMessage();
        addAiMessage(QStringLiteral("抱歉，发生了错误：%1").arg(message));
    }

    // 处理空响应
    if (m_currentAiMessage.isEmpty()) {
        removeThinkingMessage();
        addAiMessage(QStringLiteral("抱歉，我无法回复您的消息。"));
    }

    m_currentAiMessage.clear();
    m_pendingUpdateBuffer.clear();
}

void KnowledgeChatManager::flushPendingUpdates()
{
    // 如果缓冲区有待更新的内容,批量更新UI
    if (!m_pendingUpdateBuffer.isEmpty()) {
        updateLastAiMessage(m_pendingUpdateBuffer);
        m_pendingUpdateBuffer.clear();
    }
}