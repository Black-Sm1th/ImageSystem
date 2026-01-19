//#pragma once
//
//#include <QObject>
//#include <QString>
//#include <QList>
//#include <QVariantMap>
//#include <QThread>
//#include <memory>
//#include <atomic>
//
//// DCMTK 高级封装类前向声明
//class DcmSCU;
//class DcmSCP;
//class DcmDataset;
//
///**
// * @brief DICOM 网络客户端类
// * 
// * 基于 DCMTK 的 DcmSCU/DcmSCP 高级封装实现 DICOM 网络通信：
// * - C-ECHO: 测试 PACS 服务器连接
// * - C-FIND: 查询患者/检查/序列/图像
// * - C-GET:  获取 DICOM 图像（直接下载）
// * - C-MOVE: 移动 DICOM 图像到指定 AE
// * - C-STORE SCP: 接收 C-MOVE 推送的图像
// */
//class DicomNetwork : public QObject
//{
//    Q_OBJECT
//
//    // QML 属性
//    Q_PROPERTY(QString localAeTitle READ localAeTitle WRITE setLocalAeTitle NOTIFY localAeTitleChanged)
//    Q_PROPERTY(QString remoteAeTitle READ remoteAeTitle WRITE setRemoteAeTitle NOTIFY remoteAeTitleChanged)
//    Q_PROPERTY(QString remoteHost READ remoteHost WRITE setRemoteHost NOTIFY remoteHostChanged)
//    Q_PROPERTY(int remotePort READ remotePort WRITE setRemotePort NOTIFY remotePortChanged)
//    Q_PROPERTY(int localPort READ localPort WRITE setLocalPort NOTIFY localPortChanged)
//    Q_PROPERTY(QString storageDirectory READ storageDirectory WRITE setStorageDirectory NOTIFY storageDirectoryChanged)
//
//public:
//    explicit DicomNetwork(QObject* parent = nullptr);
//    ~DicomNetwork();
//
//    // 属性访问器
//    QString localAeTitle() const { return m_localAeTitle; }
//    void setLocalAeTitle(const QString& aeTitle);
//
//    QString remoteAeTitle() const { return m_remoteAeTitle; }
//    void setRemoteAeTitle(const QString& aeTitle);
//
//    QString remoteHost() const { return m_remoteHost; }
//    void setRemoteHost(const QString& host);
//
//    int remotePort() const { return m_remotePort; }
//    void setRemotePort(int port);
//
//    int localPort() const { return m_localPort; }
//    void setLocalPort(int port);
//
//    QString storageDirectory() const { return m_storageDirectory; }
//    void setStorageDirectory(const QString& dir);
//
//    // ============ C-ECHO ============
//    /**
//     * @brief 测试与 PACS 服务器的连接 (C-ECHO)
//     * @return true 如果连接成功
//     */
//    Q_INVOKABLE bool cEcho();
//
//    // ============ C-FIND ============
//    /**
//     * @brief 查询患者列表
//     * @param patientId 患者ID（可包含通配符 *）
//     * @param patientName 患者姓名（可包含通配符 *）
//     * @return 查询结果列表
//     */
//    Q_INVOKABLE QList<QVariantMap> findPatients(const QString& patientId = "*", 
//                                                 const QString& patientName = "*");
//
//    /**
//     * @brief 查询检查列表
//     * @param patientId 患者ID
//     * @param studyDate 检查日期（YYYYMMDD 或范围如 20230101-20231231）
//     * @param modality 检查模态（CT, MR, US 等）
//     * @return 查询结果列表
//     */
//    Q_INVOKABLE QList<QVariantMap> findStudies(const QString& patientId = "*",
//                                                const QString& studyDate = "*",
//                                                const QString& modality = "*");
//
//    /**
//     * @brief 查询序列列表
//     * @param studyInstanceUid 检查实例 UID
//     * @return 查询结果列表
//     */
//    Q_INVOKABLE QList<QVariantMap> findSeries(const QString& studyInstanceUid);
//
//    /**
//     * @brief 查询图像列表
//     * @param seriesInstanceUid 序列实例 UID
//     * @return 查询结果列表
//     */
//    Q_INVOKABLE QList<QVariantMap> findImages(const QString& seriesInstanceUid);
//
//    // ============ C-GET ============
//    /**
//     * @brief 使用 C-GET 获取整个检查
//     * @param studyInstanceUid 检查实例 UID
//     * @return true 如果获取成功
//     */
//    Q_INVOKABLE bool cGetStudy(const QString& studyInstanceUid);
//
//    /**
//     * @brief 使用 C-GET 获取整个序列
//     * @param seriesInstanceUid 序列实例 UID
//     * @return true 如果获取成功
//     */
//    Q_INVOKABLE bool cGetSeries(const QString& seriesInstanceUid);
//
//    // ============ C-MOVE ============
//    /**
//     * @brief 使用 C-MOVE 移动整个检查到目标 AE
//     * @param studyInstanceUid 检查实例 UID
//     * @param destinationAeTitle 目标 AE Title（默认使用本地 AE）
//     * @return true 如果移动请求成功
//     */
//    Q_INVOKABLE bool cMoveStudy(const QString& studyInstanceUid, 
//                                 const QString& destinationAeTitle = QString());
//
//    /**
//     * @brief 使用 C-MOVE 移动整个序列到目标 AE
//     * @param seriesInstanceUid 序列实例 UID
//     * @param destinationAeTitle 目标 AE Title（默认使用本地 AE）
//     * @return true 如果移动请求成功
//     */
//    Q_INVOKABLE bool cMoveSeries(const QString& seriesInstanceUid,
//                                  const QString& destinationAeTitle = QString());
//
//    // ============ C-STORE SCP ============
//    /**
//     * @brief 启动 C-STORE SCP 监听（用于接收 C-MOVE 推送的图像）
//     * @return true 如果启动成功
//     */
//    Q_INVOKABLE bool startStoreScp();
//
//    /**
//     * @brief 停止 C-STORE SCP 监听
//     */
//    Q_INVOKABLE void stopStoreScp();
//
//    /**
//     * @brief 检查 SCP 是否正在运行
//     */
//    Q_INVOKABLE bool isScpRunning() const;
//
//signals:
//    void localAeTitleChanged();
//    void remoteAeTitleChanged();
//    void remoteHostChanged();
//    void remotePortChanged();
//    void localPortChanged();
//    void storageDirectoryChanged();
//
//    // 操作进度和结果信号
//    void echoCompleted(bool success, const QString& message);
//    void findCompleted(int resultCount, const QString& message);
//    void retrieveProgress(int completed, int total);
//    void retrieveCompleted(bool success, int imageCount, const QString& message);
//    void storeScpStatusChanged(bool running, int port);
//    void imageReceived(const QString& filePath);
//    void errorOccurred(const QString& error);
//
//private:
//    // 初始化 SCU 连接
//    bool initScu(DcmSCU& scu);
//
//    // 配置参数
//    QString m_localAeTitle = "SHHZLX";
//    QString m_remoteAeTitle = "ORANTHC";
//    QString m_remoteHost = "127.0.0.1";
//    int m_remotePort = 4242;
//    int m_localPort = 8056;
//    QString m_storageDirectory;
//
//    // SCP 线程
//    class StorageScpThread;
//    std::unique_ptr<StorageScpThread> m_scpThread;
//};
//
//// 前向声明 MyStorageSCP（定义在 cpp 文件中）
//class MyStorageSCP;
//
///**
// * @brief Storage SCP 后台线程
// */
//class DicomNetwork::StorageScpThread : public QThread
//{
//    Q_OBJECT
//public:
//    StorageScpThread(const QString& aeTitle, int port, const QString& storageDir, QObject* parent = nullptr);
//    ~StorageScpThread();
//
//    void stopServer();
//
//signals:
//    void imageReceived(const QString& filePath);
//    void errorOccurred(const QString& error);
//
//protected:
//    void run() override;
//
//private:
//    QString m_aeTitle;
//    int m_port;
//    QString m_storageDir;
//    std::atomic<bool> m_stopRequested{false};
//    std::unique_ptr<MyStorageSCP> m_scp;
//};
//
