#ifndef APIMANAGER_H
#define APIMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QSet>
#include <QList>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardPaths>
#include <QDir>
#include <QUrlQuery>
#include <QCoreApplication>
#include <QTimer>
#include "CommonFunc.h"

/**
 * @brief API管理器类 - 负责处理所有网络API请求
 */
class ApiManager : public QObject
{
    Q_OBJECT
    
    /// @brief 是否使用公网，true=公网，false=内网
    QUICK_PROPERTY(bool, usePublicNetwork)
    
    SINGLETON_CLASS(ApiManager)

public:
    /**
     * @brief 用户登录请求
     * @param username 用户账号
     * @param password 用户密码
     *
     * 发送登录请求到服务器，结果通过 loginResponse 信号返回
     */
    void loginUser(const QString& username, const QString& password);
    /**
     * @brief 流式AI问答接口
     * @param query 问题内容
     * @param userId 用户ID
     * @param chatId 会话ID（可选，首次不传）
     * 
     * 发送流式问答请求到AI服务，结果通过 streamChatResponse 和 streamChatFinished 信号返回
     */
    void streamChat(const QString& query, const QString& userId, const QString& chatId = "");
    
    /**
     * @brief 知识库流式问答接口
     * @param query 问题内容
     * @param userId 用户ID
     * @param language 语言
     * @param buckets 知识库ID列表
     * @param chatId 会话ID（可选，首次不传）
     * 
     * 发送知识库流式问答请求到AI服务，结果通过 streamKnowledgeChatResponse 和 streamKnowledgeChatFinished 信号返回
     */
    void streamKnowledgeChat(const QString& query, const QString& userId, const QString& language, const QStringList& buckets, const QString& chatId = "");
    
    /**
     * @brief 删除指定的聊天记录
     * @param chatId 要删除的聊天ID
     * 
     * 发送删除聊天请求到服务器，结果通过 deleteChatResponse 信号返回
     */
    void deleteChatById(const QString& chatId);

    /**
     * @brief 终止所有正在进行的网络请求
     * 
     * 立即终止所有活跃的POST/GET请求，已发送的请求会被中断。
     * 被终止的请求不会触发对应的响应信号。
     */
    void abortAllRequests();
    
    /**
     * @brief 终止指定类型的网络请求
     * @param requestType 要终止的请求类型（如 "login", "tnm-ai-score"）
     *
     * 只终止匹配指定类型的活跃请求，其他请求继续执行。
     */
    void abortRequestsByType(const QString& requestType);

    /**
     * @brief 终止指定chatId的流式聊天请求
     * @param chatId 要终止的聊天会话ID
     *
     * 只终止匹配指定chatId的流式聊天请求，其他聊天会话继续执行。
     */
    void abortStreamChatByChatId(const QString& chatId);

signals:
    /**
     * @brief 登录响应信号
     * @param success 是否登录成功
     * @param message 服务器返回的消息
     * @param data 用户数据（登录成功时包含用户信息）
     */
    void loginResponse(bool success, const QString& message, const QJsonObject& data);

    /**
     * @brief 流式聊天数据接收信号
     * @param data 接收到的流式数据块
     * @param chatId 会话ID
     * 
     * 当接收到流式聊天数据时发出此信号，data为每次接收到的数据块
     */
    void streamChatResponse(const QString& data, const QString& chatId);
    
    /**
     * @brief 流式聊天完成信号
     * @param success 是否成功完成
     * @param message 完成消息
     * @param chatId 会话ID
     * 
     * 当流式聊天结束时发出此信号
     */
    void streamChatFinished(bool success, const QString& message, const QString& chatId);
    
    /**
     * @brief 知识库流式聊天响应信号
     * @param data 接收到的流式数据块
     * @param chatId 会话ID
     * 
     * 当接收到知识库流式聊天数据时发出此信号，data为每次接收到的数据块
     */
    void streamKnowledgeChatResponse(const QString& data, const QString& chatId);
    
    /**
     * @brief 知识库流式聊天完成信号
     * @param success 是否成功完成
     * @param message 完成消息
     * @param chatId 会话ID
     * 
     * 当知识库流式聊天结束时发出此信号
     */
    void streamKnowledgeChatFinished(bool success, const QString& message, const QString& chatId);
    
    /**
     * @brief 知识库聊天完成时的元数据信号
     * @param chatId 会话ID
     * @param retrievedMetadata 检索到的元数据列表
     * 
     * 当知识库聊天完成时，发送检索到的元数据信息
     */
    void knowledgeChatMetadataReceived(const QString& chatId, const QVariantList& retrievedMetadata);
    
    /**
     * @brief 删除聊天响应信号
     * @param success 是否删除成功
     * @param message 服务器返回的消息
     * @param data 响应数据
     */
    void deleteChatResponse(bool success, const QString& message, const QJsonObject& data);

    /**
     * @brief 网络错误信号
     * @param error 错误描述
     */
    void networkError(const QString& error);
    
    /**
     * @brief 连接测试结果信号
     * @param success 连接是否成功
     * @param message 测试结果消息
     */
    void connectionTestResult(bool success, const QString& message);

private slots:
    /**
     * @brief 网络请求完成的槽函数
     * @param reply 网络回复对象
     * 
     * 统一处理所有网络请求的响应，根据请求类型分发到对应的信号
     */
    void onNetworkReply(QNetworkReply* reply);
    
    /**
     * @brief 流式数据就绪槽函数
     * 
     * 当流式聊天接口有新数据可读时调用，处理分块接收的数据
     */
    void onStreamDataReady();
    
    /**
     * @brief 知识库流式数据就绪槽函数
     * 
     * 当知识库流式聊天接口有新数据可读时调用，处理分块接收的数据
     */
    void onStreamKnowledgeDataReady();

private:
    /**
     * @brief 获取当前使用的基础URL
     * @return 根据usePublicNetwork属性返回对应的API基础地址
     */
    QString getBaseUrl() const;
    
    /**
     * @brief 创建网络请求对象
     * @param endpoint API端点路径
     * @param setJsonContentType 是否设置JSON Content-Type，默认true
     * @return 配置好的QNetworkRequest对象
     */
    QNetworkRequest createRequest(const QString& endpoint, bool setJsonContentType = true) const;
    
    /**
     * @brief 发送POST请求
     * @param endpoint API端点路径
     * @param data 请求数据（JSON格式）
     * @param requestType 请求类型标识，用于响应时区分不同请求
     */
    void makePostRequest(const QString& endpoint, const QJsonObject& data, const QString& requestType = "");
    
    /**
     * @brief 发送GET请求
     * @param endpoint API端点路径
     * @param requestType 请求类型标识，用于响应时区分不同请求
     */
    void makeGetRequest(const QString& endpoint, const QString& requestType = "");
    
    /**
     * @brief 加载配置文件
     * 
     * 从AppData/config/config.json文件中读取网络配置，包括API地址和网络类型。
     * 如果配置文件不存在，将自动创建默认配置文件。
     * Windows系统路径示例: C:/Users/用户名/AppData/Local/组织名/应用名/config/config.json
     */
    void loadConfig();

    /// @brief Qt网络访问管理器，负责实际的网络请求
    QNetworkAccessManager* m_networkManager;
    
    /// @brief 跟踪所有活跃的网络请求，用于终止操作
    QSet<QNetworkReply*> m_activeReplies;
    
    /// @brief 跟踪流式聊天请求的chatId映射，用于在接收数据时识别会话
    QMap<QNetworkReply*, QString> m_streamChatIds;
    
    /// @brief 跟踪知识库流式聊天请求的chatId映射，用于在接收数据时识别会话
    QMap<QNetworkReply*, QString> m_streamKnowledgeChatIds;
    
    /// @brief 跟踪每个流式聊天请求的不完整SSE数据缓冲区
    QMap<QNetworkReply*, QString> m_streamDataBuffers;
    
    /// @brief 跟踪每个知识库流式聊天请求的不完整SSE数据缓冲区
    QMap<QNetworkReply*, QString> m_streamKnowledgeDataBuffers;
    
    /// @brief 跟踪每个知识库流式聊天请求的待发送内容缓冲区（用于批量发射信号）
    QMap<QNetworkReply*, QString> m_streamKnowledgePendingBuffers;
    
    /// @brief 跟踪每个知识库流式聊天请求的批量发送定时器
    QMap<QNetworkReply*, QTimer*> m_streamKnowledgeTimers;

    // API地址配置（从config.json读取）
    QString m_internalBaseUrl;  ///< 内网API基础地址
    QString m_publicBaseUrl;    ///< 公网API基础地址
};

#endif // APIMANAGER_H