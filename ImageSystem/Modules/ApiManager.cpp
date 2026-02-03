#include "ApiManager.h"

/**
 * @brief 构造函数
 * @param parent 父对象指针
 * 
 * 初始化网络管理器并从配置文件加载网络配置。
 * 连接网络管理器的finished信号到响应处理槽函数。
 */
ApiManager::ApiManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_internalBaseUrl("http://10.158.151.15:9898/api")  // 默认内网地址
    , m_publicBaseUrl("http://111.6.178.34:24603/api")   // 默认公网地址
    , m_usePublicNetwork(true)
{
    // 从配置文件加载配置
    loadConfig();
    
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &ApiManager::onNetworkReply);
}

/**
 * @brief 获取当前使用的基础URL
 * @return QString 返回内网或公网的API基础地址
 * 
 * 根据usePublicNetwork属性的值决定使用哪个网络环境。
 * true: 使用公网地址
 * false: 使用内网地址
 * 地址从config.json配置文件中读取
 */
QString ApiManager::getBaseUrl() const
{
    return getusePublicNetwork() ? m_publicBaseUrl : m_internalBaseUrl;
}

/**
 * @brief 创建标准化的网络请求对象
 * @param endpoint API端点路径（如 "/admin/user/login"）
 * @return QNetworkRequest 配置好的请求对象
 * 
 * 设置统一的请求头信息：
 * - Content-Type: application/json
 * - User-Agent: ScoreReport/1.0
 * - 完整的请求URL = baseUrl + endpoint
 */
QNetworkRequest ApiManager::createRequest(const QString& endpoint, bool setJsonContentType) const
{
    QUrl url(getBaseUrl() + endpoint);
    QNetworkRequest request(url);
    
    // 只有在需要时才设置JSON Content-Type（文件上传不需要）
    if (setJsonContentType) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }
    
    request.setRawHeader("User-Agent", "ScoreReport/1.0");
    
    qDebug() << "[ApiManager] Creating request to:" << url.toString();
    return request;
}

/**
 * @brief 发送POST请求的通用方法
 * @param endpoint API端点路径
 * @param data 要发送的JSON数据
 * @param requestType 请求类型标识，用于在响应时区分不同的请求
 * 
 * 将JSON数据序列化为字节数组并发送POST请求。
 * requestType会被添加到请求头中，便于在onNetworkReply中识别响应类型。
 */
void ApiManager::makePostRequest(const QString& endpoint, const QJsonObject& data, const QString& requestType)
{
    QNetworkRequest request = createRequest(endpoint);
    
    // 添加请求类型标识，用于在回复中区分不同的请求
    if (!requestType.isEmpty()) {
        request.setRawHeader("X-Request-Type", requestType.toUtf8());
    }
    
    QByteArray body = QJsonDocument(data).toJson(QJsonDocument::Indented);
    qDebug().noquote() << "[ApiManager] POST request body:" << QString::fromUtf8(body);
    
    QNetworkReply* reply = m_networkManager->post(request, body);
    m_activeReplies.insert(reply);  // 跟踪活跃的请求
}

/**
 * @brief 发送GET请求的通用方法
 * @param endpoint API端点路径
 * @param requestType 请求类型标识
 * 
 * 发送GET请求，主要用于查询操作。
 */
void ApiManager::makeGetRequest(const QString& endpoint, const QString& requestType)
{
    QNetworkRequest request = createRequest(endpoint);
    
    if (!requestType.isEmpty()) {
        request.setRawHeader("X-Request-Type", requestType.toUtf8());
    }
    
    QNetworkReply* reply = m_networkManager->get(request);
    m_activeReplies.insert(reply);  // 跟踪活跃的请求
}

void ApiManager::loginUser(const QString& username, const QString& password)
{
    QJsonObject loginData;
    loginData["userAccount"] = username;
    loginData["userPassword"] = password;

    makePostRequest("/admin/user/login", loginData, "login");
}

/**
 * @brief 流式AI问答接口实现
 * @param query 问题内容
 * @param userId 当前用户ID
 * @param chatId 会话ID（可选，首次对话时为空）
 * 
 * 发送流式问答请求到AI服务。
 * 这是一个特殊的接口，响应数据以流的形式分块返回，需要监听readyRead信号。
 * 数据通过 streamChatResponse 信号逐块返回，完成时通过 streamChatFinished 信号通知。
 */
void ApiManager::streamChat(const QString& query, const QString& userId, const QString& chatId)
{
    QJsonObject requestData;
    requestData["query"] = query;
    requestData["userId"] = userId;
    
    // chatId为可选参数，只有在不为空时才添加
    if (!chatId.isEmpty()) {
        requestData["chatId"] = chatId;
    }
    
    QNetworkRequest request = createRequest("/admin/Ai/chat");
    request.setRawHeader("X-Request-Type", "stream-chat");
    
    QByteArray body = QJsonDocument(requestData).toJson(QJsonDocument::Indented);
    qDebug().noquote() << "[ApiManager] Stream chat request body:" << QString::fromUtf8(body);
    
    QNetworkReply* reply = m_networkManager->post(request, body);
    m_activeReplies.insert(reply);
    
    // 保存chatId映射，用于在接收数据时识别会话
    m_streamChatIds[reply] = chatId;
    
    // 连接流式数据读取信号
    connect(reply, &QNetworkReply::readyRead, this, &ApiManager::onStreamDataReady);
}

/**
 * @brief 知识库流式问答接口实现
 * @param query 问题内容
 * @param userId 用户ID
 * @param language 语言
 * @param buckets 知识库ID列表
 * @param chatId 会话ID（可选，首次不传）
 * 
 * 发送知识库流式问答请求到服务器的 /admin/AI/doc/chat 端点。
 * 请求类型标记为 "stream-knowledge-chat"，结果会通过 streamKnowledgeChatResponse 和 streamKnowledgeChatFinished 信号返回。
 */
void ApiManager::streamKnowledgeChat(const QString& query, const QString& userId, const QString& language, const QStringList& buckets, const QString& chatId)
{
    QJsonObject requestData;
    requestData["query"] = query;
    requestData["userId"] = userId;
    requestData["language"] = language;
    // 转换buckets为JSON数组
    QJsonArray bucketsArray;
    for (const QString& bucket : buckets) {
        bucketsArray.append(bucket);
    }
    requestData["buckets"] = bucketsArray;
    
    // chatId为可选参数，只有在不为空时才添加
    if (!chatId.isEmpty()) {
        requestData["chatId"] = chatId;
    }
    
    QNetworkRequest request = createRequest("/admin/Ai/doc/chat");
    request.setRawHeader("X-Request-Type", "stream-knowledge-chat");
    
    QByteArray body = QJsonDocument(requestData).toJson(QJsonDocument::Indented);
    qDebug().noquote() << "[ApiManager] Stream knowledge chat request body:" << QString::fromUtf8(body);
    
    QNetworkReply* reply = m_networkManager->post(request, body);
    m_activeReplies.insert(reply);
    
    // 保存chatId映射，用于在接收数据时识别会话
    m_streamKnowledgeChatIds[reply] = chatId;
    
    // 连接流式数据读取信号
    connect(reply, &QNetworkReply::readyRead, this, &ApiManager::onStreamKnowledgeDataReady);
}

/**
 * @brief 删除聊天接口实现
 * @param chatId 要删除的聊天ID
 * 
 * 发送删除聊天请求到服务器的 /admin/Ai/delete/chat 端点。
 * 请求类型标记为 "delete-chat"，结果会通过 deleteChatResponse 信号返回。
 */
void ApiManager::deleteChatById(const QString& chatId)
{
    QJsonObject requestData;
    requestData["chatId"] = chatId;

    makePostRequest("/admin/Ai/delete/chat", requestData, "delete-chat");
}

/**
 * @brief 流式数据就绪槽函数实现
 * 
 * 当流式聊天接口有新数据可读时调用此函数。
 * 读取当前可用的数据块并通过 streamChatResponse 信号发出。
 * 处理Server-Sent Events (SSE) 格式的数据。
 */
void ApiManager::onStreamDataReady()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qWarning() << "[ApiManager] onStreamDataReady: Invalid sender";
        return;
    }
    
    // 获取对应的chatId
    QString chatId = m_streamChatIds.value(reply, "");
    
    // 读取所有可用数据
    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        return;
    }
    
    QString newDataString = QString::fromUtf8(data);
    qDebug() << "[ApiManager] Stream data received:" << newDataString;
    
    // 将新数据追加到缓冲区
    QString& buffer = m_streamDataBuffers[reply];
    buffer += newDataString;
    
    // 处理换行符：先将额外的换行符标记出来，然后按标准SSE格式分割
    QString processedBuffer = buffer;
    
    // 将 \n\n\n\n 替换为 <<DOUBLE_NEWLINE>>\n\n (保留两个换行符)
    processedBuffer.replace("\n\n\n\n", "<<DOUBLE_NEWLINE>>\n\n");
    
    // 将 \n\n\n 替换为 <<SINGLE_NEWLINE>>\n\n (保留一个换行符)  
    processedBuffer.replace("\n\n\n", "<<SINGLE_NEWLINE>>\n\n");
    
    // 按标准SSE格式分割事件
    QStringList events = processedBuffer.split("\n\n");
    
    // 保留最后一个可能不完整的事件在缓冲区中
    if (!processedBuffer.endsWith("\n\n")) {
        // 最后一个事件可能不完整，保留在缓冲区中
        buffer = events.takeLast();
        // 恢复原始的换行符标记到缓冲区
        buffer.replace("<<DOUBLE_NEWLINE>>", "\n\n");
        buffer.replace("<<SINGLE_NEWLINE>>", "\n");
    } else {
        // 如果以双换行符结尾，说明所有事件都是完整的
        buffer.clear();
    }
    
    // 处理完整的SSE事件
    for (const QString& event : events) {
        if (event.trimmed().isEmpty()) {
            continue;
        }
        
        QStringList lines = event.split('\n');
        QString eventType = "";
        QString content = "";
        
        for (const QString& line : lines) {
            // 处理 event: 行
            if (line.trimmed().startsWith("event:")) {
                eventType = line.trimmed().mid(6).trimmed();
            }
            // 处理 data: 行（保持原始格式，不要trim）
            else if (line.startsWith("data:")) {
                // 从 "data:" 后面开始取所有内容，保留空格
                content = line.mid(5);
                
                // 恢复额外的换行符
                content.replace("<<DOUBLE_NEWLINE>>", "\n\n");
                content.replace("<<SINGLE_NEWLINE>>", "\n");
            }
        }
        
        // 处理完整的SSE事件
        if (!eventType.isEmpty() && !content.isEmpty()) {
            if (content != "[DONE]") {
                if (eventType == "message") {
                    // 消息事件，直接发送文本内容（保留所有空格）
                    qDebug() << "[ApiManager] Sending content:" << QStringLiteral("'%1'").arg(content) << "Length:" << content.length();
                    emit streamChatResponse(content, chatId);
                } else if (eventType == "complete") {
                    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
                    if (doc.isObject()) {
                        QJsonObject obj = doc.object();
                        QString string = obj["content"].toString();
                        emit streamChatFinished(true, string, chatId);
                    }
                    // 清理映射和缓冲区
                    m_streamChatIds.remove(reply);
                    m_streamDataBuffers.remove(reply);
                    return; // 完成后退出
                } else {
                    // 其他事件，尝试解析JSON数据
                    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
                    if (doc.isObject()) {
                        QJsonObject obj = doc.object();
                        QString text = obj.value("content").toString();
                        if (!text.isEmpty()) {
                            emit streamChatResponse(text, chatId);
                        }
                    } else {
                        // 如果不是JSON，直接发送文本内容（保留空格）
                        emit streamChatResponse(content, chatId);
                    }
                }
            }
        }
    }
}

/**
 * @brief 知识库流式数据就绪槽函数实现
 * 
 * 当知识库流式聊天接口有新数据可读时调用此函数。
 * 读取当前可用的数据块并通过 streamKnowledgeChatResponse 信号发出。
 * 处理Server-Sent Events (SSE) 格式的数据。
 */
void ApiManager::onStreamKnowledgeDataReady()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qWarning() << "[ApiManager] onStreamKnowledgeDataReady: Invalid sender";
        return;
    }
    
    // 获取对应的chatId
    QString chatId = m_streamKnowledgeChatIds.value(reply, "");
    
    // 读取所有可用数据
    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        return;
    }
    
    QString newDataString = QString::fromUtf8(data);
    qDebug() << "[ApiManager] Knowledge stream data received:" << newDataString;
    
    // 将新数据追加到缓冲区
    QString& buffer = m_streamKnowledgeDataBuffers[reply];
    buffer += newDataString;
    
    // 处理换行符：先将额外的换行符标记出来，然后按标准SSE格式分割
    QString processedBuffer = buffer;
    
    // 将 \n\n\n\n 替换为 <<DOUBLE_NEWLINE>>\n\n (保留两个换行符)
    processedBuffer.replace("\n\n\n\n", "<<DOUBLE_NEWLINE>>\n\n");
    
    // 将 \n\n\n 替换为 <<SINGLE_NEWLINE>>\n\n (保留一个换行符)  
    processedBuffer.replace("\n\n\n", "<<SINGLE_NEWLINE>>\n\n");
    
    // 按标准SSE格式分割事件
    QStringList events = processedBuffer.split("\n\n");
    
    // 保留最后一个可能不完整的事件在缓冲区中
    if (!processedBuffer.endsWith("\n\n")) {
        // 最后一个事件可能不完整，保留在缓冲区中
        buffer = events.takeLast();
        // 恢复原始的换行符标记到缓冲区
        buffer.replace("<<DOUBLE_NEWLINE>>", "\n\n");
        buffer.replace("<<SINGLE_NEWLINE>>", "\n");
    } else {
        // 如果以双换行符结尾，说明所有事件都是完整的
        buffer.clear();
    }
    
    // 处理完整的SSE事件
    for (const QString& event : events) {
        if (event.trimmed().isEmpty()) {
            continue;
        }
        
        QStringList lines = event.split('\n');
        QString eventType = "";
        QString content = "";
        
        for (const QString& line : lines) {
            // 处理 event: 行
            if (line.trimmed().startsWith("event:")) {
                eventType = line.trimmed().mid(6).trimmed();
            }
            // 处理 data: 行（保持原始格式，不要trim）
            else if (line.startsWith("data:")) {
                // 从 "data:" 后面开始取所有内容，保留空格
                content = line.mid(5);
                
                // 恢复额外的换行符
                content.replace("<<DOUBLE_NEWLINE>>", "\n\n");
                content.replace("<<SINGLE_NEWLINE>>", "\n");
            }
        }
        
        // 处理完整的SSE事件
        if (!eventType.isEmpty() && !content.isEmpty()) {
            if (content != "[DONE]") {
                if (eventType == "message") {
                    // 消息事件，累积到待发送缓冲区而不是立即发送
                    qDebug() << "[ApiManager] Knowledge buffering content:" << QStringLiteral("'%1'").arg(content) << "Length:" << content.length();
                    m_streamKnowledgePendingBuffers[reply] += content;
                    
                    // 如果定时器不存在，创建并启动
                    if (!m_streamKnowledgeTimers.contains(reply)) {
                        QTimer* timer = new QTimer(this);
                        timer->setInterval(30);  // 每30ms批量发送一次
                        timer->setSingleShot(true);
                        m_streamKnowledgeTimers[reply] = timer;
                        
                        // 连接定时器信号，使用lambda捕获reply指针
                        connect(timer, &QTimer::timeout, this, [this, reply]() {
                            // 检查reply是否仍然有效
                            if (m_streamKnowledgePendingBuffers.contains(reply)) {
                                QString bufferedContent = m_streamKnowledgePendingBuffers[reply];
                                if (!bufferedContent.isEmpty()) {
                                    QString chatId = m_streamKnowledgeChatIds.value(reply, "");
                                    qDebug() << "[ApiManager] Knowledge sending batched content, Length:" << bufferedContent.length();
                                    emit streamKnowledgeChatResponse(bufferedContent, chatId);
                                    m_streamKnowledgePendingBuffers[reply].clear();
                                }
                            }
                        });
                    }
                    
                    // 启动或重启定时器
                    QTimer* timer = m_streamKnowledgeTimers[reply];
                    if (!timer->isActive()) {
                        timer->start();
                    }
                } else if (eventType == "complete") {
                    // 完成事件，先立即刷新所有待发送的内容
                    if (m_streamKnowledgeTimers.contains(reply)) {
                        QTimer* timer = m_streamKnowledgeTimers[reply];
                        timer->stop();
                        
                        // 立即发送所有缓冲的内容
                        if (m_streamKnowledgePendingBuffers.contains(reply)) {
                            QString bufferedContent = m_streamKnowledgePendingBuffers[reply];
                            if (!bufferedContent.isEmpty()) {
                                qDebug() << "[ApiManager] Knowledge flushing final content, Length:" << bufferedContent.length();
                                emit streamKnowledgeChatResponse(bufferedContent, chatId);
                            }
                        }
                        
                        // 清理定时器
                        timer->deleteLater();
                        m_streamKnowledgeTimers.remove(reply);
                    }
                    
                    // 完成事件，解析JSON数据并发送元数据
                    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
                    if (doc.isObject()) {
                        QJsonObject obj = doc.object();
                        
                        // 提取retrieved_metadata
                        if (obj.contains("retrieved_metadata")) {
                            QJsonArray metadataArray = obj["retrieved_metadata"].toArray();
                            QVariantList metadataList;
                            
                            for (const QJsonValue& value : metadataArray) {
                                if (value.isObject()) {
                                    QJsonObject metaObj = value.toObject();
                                    QVariantMap metaMap;
                                    metaMap["retriever_name"] = metaObj["retriever_name"].toString();
                                    metaMap["url"] = metaObj["url"].toString();
                                    metaMap["file_name"] = metaObj["file_name"].toString();
                                    
                                    // 处理页码数组
                                    if (metaObj.contains("page_numbers") && metaObj["page_numbers"].isArray()) {
                                        QJsonArray pageArray = metaObj["page_numbers"].toArray();
                                        QVariantList pageList;
                                        for (const QJsonValue& pageValue : pageArray) {
                                            pageList.append(pageValue.toInt());
                                        }
                                        metaMap["page_numbers"] = pageList;
                                    }
                                    
                                    metadataList.append(metaMap);
                                }
                            }
                            
                            // 发送元数据信号
                            emit knowledgeChatMetadataReceived(chatId, metadataList);
                        }
                    }
                    
                    // 发送完成信号
                    emit streamKnowledgeChatFinished(true, "知识库聊天完成", chatId);
                    // 清理映射和缓冲区
                    m_streamKnowledgeChatIds.remove(reply);
                    m_streamKnowledgeDataBuffers.remove(reply);
                    m_streamKnowledgePendingBuffers.remove(reply);
                    return; // 完成后退出
                } else {
                    // 其他事件，尝试解析JSON数据
                    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
                    if (doc.isObject()) {
                        QJsonObject obj = doc.object();
                        QString text = obj.value("content").toString();
                        if (!text.isEmpty()) {
                            emit streamKnowledgeChatResponse(text, chatId);
                        }
                    } else {
                        // 如果不是JSON，直接发送文本内容（保留空格）
                        emit streamKnowledgeChatResponse(content, chatId);
                    }
                }
            }
        }
    }
}

/**
 * @brief 网络请求响应的统一处理函数
 * @param reply 网络回复对象
 * 
 * 这是所有网络请求的统一响应处理入口，主要功能：
 * 1. 从请求头中获取请求类型标识
 * 2. 检查网络错误
 * 3. 解析JSON响应数据
 * 4. 根据请求类型分发到对应的信号
 * 5. 清理回复对象
 * 
 * API响应格式：
 * {
 *   "code": 0,        // 0表示成功，非0表示失败
 *   "message": "",    // 消息描述
 *   "data": {}        // 具体数据
 * }
 */
void ApiManager::onNetworkReply(QNetworkReply* reply)
{
    QString requestType = QString::fromUtf8(reply->request().rawHeader("X-Request-Type"));
    QUrl replyUrl = reply->url();
    
    qDebug() << "[ApiManager] Reply received from:" << replyUrl.toString() 
             << "Type:" << requestType;
    
    // 从活跃请求集合中移除
    m_activeReplies.remove(reply);
    
    if (reply->error() == QNetworkReply::NoError) {
        // 网络请求成功，解析响应数据
        QByteArray responseData = reply->readAll();
        qDebug().noquote() << "[ApiManager] Response data:" << QString::fromUtf8(responseData);
        
        // 对于流式聊天请求，特殊处理
        if (requestType == "stream-chat") {
            // 清理chatId映射和缓冲区
            m_streamChatIds.remove(reply);
            m_streamDataBuffers.remove(reply);
        } else if (requestType == "stream-knowledge-chat") {
            // 知识库流式聊天完成，发送完成信号
            QString chatId = m_streamKnowledgeChatIds.value(reply, "");
            emit streamKnowledgeChatFinished(true, "知识库聊天完成", chatId);
            // 清理chatId映射和缓冲区
            m_streamKnowledgeChatIds.remove(reply);
            m_streamKnowledgeDataBuffers.remove(reply);
        } else {
            // 其他请求需要解析JSON响应
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            if (!doc.isObject()) {
                qWarning() << "[ApiManager] Invalid JSON response";
                emit networkError("Invalid server response");
            } else {
                QJsonObject responseObj = doc.object();
                int code = responseObj.value("code").toInt();
                QString message = responseObj.value("message").toString();
                QJsonObject data = responseObj.value("data").toObject();
                bool success = (code == 0);  // 服务器约定：code为0表示成功
     
                // 根据请求类型分发响应到对应的信号
                if (requestType == "login") {
                    emit loginResponse(success, message, data);
                } else if (requestType == "test-connection") {
                    emit connectionTestResult(success, message);
                } else if (requestType == "delete-chat") {
                    emit deleteChatResponse(success, message, data);
                } 
            }
        }
    } else {
        // 网络请求失败，处理错误
        QString errorString = reply->errorString();
        qWarning() << "[ApiManager] Network error:" << errorString;
        
        // 检查是否是手动终止的请求
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            qDebug() << "[ApiManager] Request was manually aborted:" << requestType;
            // 被终止的请求不发送错误信号，直接清理即可
        } else {
            // 根据请求类型发送错误响应
            if (requestType == "login") {
                emit loginResponse(false, errorString, QJsonObject());
            } else if (requestType == "test-connection") {
                emit connectionTestResult(false, errorString);
            } else if (requestType == "delete-chat") {
                emit deleteChatResponse(false, errorString, QJsonObject());
            } else if (requestType == "stream-chat") {
                // 流式聊天错误，发送错误完成信号
                QString chatId = m_streamChatIds.value(reply, "");
                emit streamChatFinished(false, errorString, chatId);
                // 清理chatId映射和缓冲区
                m_streamChatIds.remove(reply);
                m_streamDataBuffers.remove(reply);
            } else if (requestType == "stream-knowledge-chat") {
                // 知识库流式聊天错误，发送错误完成信号
                QString chatId = m_streamKnowledgeChatIds.value(reply, "");
                emit streamKnowledgeChatFinished(false, errorString, chatId);
                // 清理chatId映射和缓冲区
                m_streamKnowledgeChatIds.remove(reply);
                m_streamKnowledgeDataBuffers.remove(reply);
            } else {
                emit networkError(errorString);
            }
        }
    }
    
    // 清理网络回复对象，防止内存泄漏
    reply->deleteLater();
}

/**
 * @brief 终止所有正在进行的网络请求
 * 
 * 遍历所有活跃的请求并调用abort()方法终止它们。
 * 被终止的请求会触发OperationCanceledError，但不会发送错误信号。
 * 这个方法通常在用户取消操作或应用程序退出时调用。
 */
void ApiManager::abortAllRequests()
{
    qDebug() << "[ApiManager] Aborting all active requests, count:" << m_activeReplies.size();
    
    // 复制集合，因为abort()会触发finished信号，导致集合在遍历时被修改
    QSet<QNetworkReply*> repliesToAbort = m_activeReplies;
    
    for (QNetworkReply* reply : repliesToAbort) {
        if (reply && reply->isRunning()) {
            qDebug() << "[ApiManager] Aborting request to:" << reply->url().toString();
            reply->abort();
        }
    }
    
    // 清理所有流式聊天的chatId映射和缓冲区
    m_streamChatIds.clear();
    m_streamDataBuffers.clear();
}

/**
 * @brief 终止指定类型的网络请求
 * @param requestType 要终止的请求类型（如 "login", "tnm-ai-score"）
 * 
 * 只终止匹配指定类型的活跃请求，允许对特定操作进行精确控制。
 * 例如：abortRequestsByType("login") 只会终止登录请求，其他请求继续执行。
 */
void ApiManager::abortRequestsByType(const QString& requestType)
{
    qDebug() << "[ApiManager] Aborting requests of type:" << requestType;
    
    // 复制集合避免遍历时修改
    QSet<QNetworkReply*> repliesToCheck = m_activeReplies;
    
    for (QNetworkReply* reply : repliesToCheck) {
        if (reply && reply->isRunning()) {
            QString replyType = QString::fromUtf8(reply->request().rawHeader("X-Request-Type"));
            if (replyType == requestType) {
                qDebug() << "[ApiManager] Aborting request:" << reply->url().toString() 
                         << "Type:" << replyType;
                reply->abort();
                
                // 如果是流式聊天请求，清理对应的chatId映射和缓冲区
                if (replyType == "stream-chat") {
                    m_streamChatIds.remove(reply);
                    m_streamDataBuffers.remove(reply);
                } else if (replyType == "stream-knowledge-chat") {
                    m_streamKnowledgeChatIds.remove(reply);
                    m_streamKnowledgeDataBuffers.remove(reply);
                }
            }
        }
    }
}

/**
 * @brief 终止指定chatId的流式聊天请求
 * @param chatId 要终止的聊天会话ID
 *
 * 只终止匹配指定chatId的流式聊天请求，其他聊天会话继续执行。
 * 这样可以避免一个ChatManager实例影响其他实例的对话。
 */
void ApiManager::abortStreamChatByChatId(const QString& chatId)
{
    qDebug() << "[ApiManager] Aborting stream chat requests for chatId:" << chatId;

    // 复制集合避免遍历时修改
    QSet<QNetworkReply*> repliesToCheck = m_activeReplies;

    for (QNetworkReply* reply : repliesToCheck) {
        if (reply && reply->isRunning()) {
            QString replyType = QString::fromUtf8(reply->request().rawHeader("X-Request-Type"));
            QString replyChatId = m_streamChatIds.value(reply, "");

            // 只中断匹配chatId的流式聊天请求
            if (replyType == "stream-chat" && replyChatId == chatId) {
                qDebug() << "[ApiManager] Aborting stream chat request:" << reply->url().toString()
                         << "ChatId:" << replyChatId;
                reply->abort();

                // 清理对应的chatId映射和缓冲区
                m_streamChatIds.remove(reply);
                m_streamDataBuffers.remove(reply);
            } else if (replyType == "stream-knowledge-chat" && m_streamKnowledgeChatIds.value(reply, "") == chatId) {
                qDebug() << "[ApiManager] Aborting stream knowledge chat request:" << reply->url().toString()
                         << "ChatId:" << chatId;
                reply->abort();

                // 清理对应的chatId映射和缓冲区
                m_streamKnowledgeChatIds.remove(reply);
                m_streamKnowledgeDataBuffers.remove(reply);
                m_streamKnowledgePendingBuffers.remove(reply);
                
                // 清理定时器
                if (m_streamKnowledgeTimers.contains(reply)) {
                    QTimer* timer = m_streamKnowledgeTimers[reply];
                    timer->stop();
                    timer->deleteLater();
                    m_streamKnowledgeTimers.remove(reply);
                }
            }
        }
    }
}

/**
 * @brief 加载配置文件
 * 
 * 从AppData/config/config.json文件中读取网络配置，包括API地址和网络类型。
 * 如果配置文件不存在，将自动创建默认配置文件。
 */
void ApiManager::loadConfig()
{
    QString configDir = "AppData/config/";
    QString configPath = configDir + "config.json";
    
    QFile configFile(configPath);
    
    // 如果配置文件不存在，创建默认配置
    if (!configFile.exists()) {
        // 确保config目录存在
        QDir dir;
        if (!dir.mkpath(configDir)) {
            setusePublicNetwork(true);  // 默认使用公网
            return;
        }
        
        // 创建默认配置对象
        QJsonObject networkObj;
        networkObj["usePublicNetwork"] = true;
        networkObj["internalBaseUrl"] = m_internalBaseUrl;
        networkObj["publicBaseUrl"] = m_publicBaseUrl;
        
        QJsonObject rootObj;
        rootObj["network"] = networkObj;
        
        // 写入配置文件
        if (configFile.open(QIODevice::WriteOnly)) {
            QJsonDocument doc(rootObj);
            configFile.write(doc.toJson(QJsonDocument::Indented));
            configFile.close();
        } else {
            setusePublicNetwork(true);  // 默认使用公网
            return;
        }
    }
    
    // 读取配置文件
    if (!configFile.open(QIODevice::ReadOnly)) {
        setusePublicNetwork(true);  // 默认使用公网
        return;
    }
    
    QByteArray configData = configFile.readAll();
    configFile.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(configData);
    if (!doc.isObject()) {
        setusePublicNetwork(true);  // 默认使用公网
        return;
    }
    
    QJsonObject rootObj = doc.object();
    if (!rootObj.contains("network")) {
        setusePublicNetwork(true);  // 默认使用公网
        return;
    }
    
    QJsonObject networkObj = rootObj["network"].toObject();
    
    // 读取网络类型配置
    if (networkObj.contains("usePublicNetwork")) {
        bool usePublic = networkObj["usePublicNetwork"].toBool();
        setusePublicNetwork(usePublic);
    } else {
        setusePublicNetwork(true);  // 默认使用公网
    }
    
    // 读取内网地址配置
    if (networkObj.contains("internalBaseUrl")) {
        m_internalBaseUrl = networkObj["internalBaseUrl"].toString();
    }
    
    // 读取公网地址配置
    if (networkObj.contains("publicBaseUrl")) {
        m_publicBaseUrl = networkObj["publicBaseUrl"].toString();
    }
}
