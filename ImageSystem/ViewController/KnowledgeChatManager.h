#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QJsonObject>
#include <QThread>
#include <QMutex>
#include "Modules/CommonFunc.h"

/**
 * @brief 聊天管理器类 - 负责处理聊天功能
 *
 * 提供聊天消息管理和API调用功能，支持多实例
 */
class KnowledgeChatManager : public QObject
{
    Q_OBJECT
        SINGLETON_CLASS(KnowledgeChatManager)

        /// @brief 消息列表，供QML绑定
        QUICK_PROPERTY(QVariantList, messages)

        /// @brief 是否正在发送消息
        QUICK_PROPERTY(bool, isSending)

        /// @brief 是否显示思考中状态
        QUICK_PROPERTY(bool, isThinking)

        /// @brief 当前聊天ID
        QUICK_PROPERTY(QString, currentChatId)

        /// @brief 最后一条用户消息
        QUICK_PROPERTY(QString, lastUserMessage)
public:
    // QML调用的方法
    Q_INVOKABLE void sendMessage(const QString& message);
    Q_INVOKABLE void regenerateLastResponse();
    Q_INVOKABLE void endAnalysis(bool clearfile);
    Q_INVOKABLE void copyToClipboard(const QString& content);

private slots:
    /**
     * @brief 处理流式聊天响应
     * @param data 接收到的数据块
     * @param chatId 会话ID
     */
    void onStreamChatResponse(const QString& data, const QString& chatId);

    /**
     * @brief 处理流式聊天完成
     * @param success 是否成功
     * @param message 完成消息
     * @param chatId 会话ID
     */
    void onStreamChatFinished(bool success, const QString& message, const QString& chatId);

    /**
     * @brief 处理知识库流式聊天响应
     * @param data 接收到的数据块
     * @param chatId 会话ID
     */
    void onStreamKnowledgeChatResponse(const QString& data, const QString& chatId);

    /**
     * @brief 处理知识库流式聊天完成
     * @param success 是否成功
     * @param message 完成消息
     * @param chatId 会话ID
     */
    void onStreamKnowledgeChatFinished(bool success, const QString& message, const QString& chatId);

private:
    // 消息管理私有方法
    void addUserMessage(const QString& message);
    void addAiMessage(const QString& message);
    void addThinkingMessage();
    void removeThinkingMessage();
    void updateLastAiMessage(const QString& additionalText);
    // UI更新优化私有方法
    void flushPendingUpdates();
    QString m_currentAiMessage;                                 ///< 当前正在接收的AI消息内容
    QString m_pendingUpdateBuffer;                               ///< 待更新的内容缓冲区
    QTimer* m_updateTimer;                                       ///< UI更新定时器
};