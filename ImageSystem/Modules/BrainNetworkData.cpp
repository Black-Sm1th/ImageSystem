#include "BrainNetworkData.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include <QUrl>

BrainNetworkData::BrainNetworkData(QObject* parent)
    : QObject(parent)
{
}

BrainNetworkData::~BrainNetworkData() = default;

bool BrainNetworkData::loadFromFolder(const QString& folderPath)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        emit errorOccurred("文件夹不存在: " + folderPath);
        return false;
    }

    QString jsonPath = dir.filePath("brain_network_results.json");
    if (!QFile::exists(jsonPath)) {
        emit errorOccurred("未找到 brain_network_results.json");
        return false;
    }

    emit loadProgress(5);

    if (!loadJson(jsonPath)) {
        return false;
    }

    emit loadProgress(60);

    if (!loadImages(folderPath)) {
        return false;
    }

    emit loadProgress(100);
    emit loadFinished(true);
    return true;
}

bool BrainNetworkData::loadJson(const QString& jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("无法打开JSON文件");
        return false;
    }

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        emit errorOccurred("JSON解析错误: " + err.errorString());
        return false;
    }

    QJsonArray regionArray;
    QJsonObject globalMetrics;

    if (doc.isArray()) {
        regionArray = doc.array();
    }
    else if (doc.isObject()) {
        QJsonObject root = doc.object();
        regionArray = root.value("regions").toArray();
        globalMetrics = root.value("global_metrics").toObject();
        if (regionArray.isEmpty()) {
            emit errorOccurred("JSON格式错误：缺少 regions 数组");
            return false;
        }
    }
    else {
        emit errorOccurred("JSON格式错误：根必须是数组或对象");
        return false;
    }

    if (regionArray.size() != 116) {
        qWarning() << "警告：脑区数量不是116，实际为" << regionArray.size();
    }

    m_regions.clear();
    m_rankHash.clear();
    m_chineseHash.clear();
    m_englishHash.clear();

    m_regions.reserve(116);
    m_rankHash.reserve(128);
    m_chineseHash.reserve(128);
    m_englishHash.reserve(128);

    double sumDeg = 0, sumClu = 0, sumEff = 0, sumAlff = 0;

    for (const auto& v : regionArray) {
        QJsonObject o = v.toObject();

        BrainRegion r;
        r.rank = o["rank"].toInt();
        r.chineseName = o["chinese_name"].toString();
        r.englishName = o["english_name"].toString();
        r.degree = o["degree"].toInt();
        r.clustering = o["clustering_coefficient"].toDouble();
        r.localEfficiency = o["local_efficiency"].toDouble();
        r.alff = o["alff"].toDouble();
        r.timeSeriesImagePath = o["time_series_image"].toString();

        m_regions.append(r);

        // 哈希表填充（指针指向 m_regions 中的对象，生命周期安全）
        const BrainRegion* ptr = &m_regions.last();
        m_rankHash.insert(r.rank, ptr);
        m_chineseHash.insert(r.chineseName, ptr);
        m_englishHash.insert(r.englishName, ptr);

        sumDeg += r.degree;
        sumClu += r.clustering;
        sumEff += r.localEfficiency;
        sumAlff += r.alff;
    }

    int n = m_regions.size();
    m_avgDegree = sumDeg / n;
    m_avgClustering = sumClu / n;
    m_avgLocalEff = sumEff / n;
    m_avgALFF = sumAlff / n;

    if (!globalMetrics.isEmpty()) {
        if (globalMetrics.contains("average_clustering_coefficient")) {
            m_avgClustering = globalMetrics.value("average_clustering_coefficient").toDouble();
        }
        if (globalMetrics.contains("average_local_efficiency")) {
            m_avgLocalEff = globalMetrics.value("average_local_efficiency").toDouble();
        }

        m_globalEfficiency = globalMetrics.value("global_efficiency").toDouble();
        m_richClubPercentage = globalMetrics.value("rich_club_percentage").toDouble();
        m_bridgePercentage = globalMetrics.value("bridge_percentage").toDouble();
        m_localPercentage = globalMetrics.value("local_percentage").toDouble();
    }
    else {
        m_globalEfficiency = 0.0;
        m_richClubPercentage = 0.0;
        m_bridgePercentage = 0.0;
        m_localPercentage = 0.0;
    }

    return true;
}

bool BrainNetworkData::loadImages(const QString& folderPath)
{
    QDir dir(folderPath);

    // ============ covariance.png ============
    QString covPath = dir.filePath("covariance.png");
    if (QFile::exists(covPath)) {
        QImage img(covPath);                     // 先用 QImage 读取（任何线程都安全）
        if (!img.isNull()) {
            m_covariancePixmap = QPixmap::fromImage(img);
            qDebug() << "covariance.png 加载成功";
        }
        else {
            qWarning() << "covariance.png 读取失败（文件损坏或格式不支持）:" << covPath;
        }
    }
    else {
        qWarning() << "covariance.png 不存在:" << covPath;
    }

    // ============ alff.png ============
    QString alffPath = dir.filePath("alff.png");
    if (QFile::exists(alffPath)) {
        QImage img(alffPath);
        if (!img.isNull()) {
            m_alffPixmap = QPixmap::fromImage(img);
        }
        else {
            qWarning() << "alff.png 读取失败:" << alffPath;
        }
    }

    // ============ viewConnectome.html ============
    QString htmlPath = dir.filePath("viewConnectome.html");
    if (QFile::exists(htmlPath)) {
        m_connectomeHtmlPath = QUrl::fromLocalFile(htmlPath).toString();
    }

    // ============ 116 张时间序列图（同样用 QImage，永不崩溃） ============
    QDir plotsDir(dir.filePath("region_plots"));
    if (plotsDir.exists()) {
        for (auto& r : m_regions) {
            // timeSeriesImagePath 是相对路径，例如 "region_plots/001_Precentral_L.png"
            QString fullPath = plotsDir.filePath(QFileInfo(r.timeSeriesImagePath).fileName());

            if (QFile::exists(fullPath)) {
                QImage img(fullPath);
                if (!img.isNull()) {
                    r.timeSeriesPixmap = QPixmap::fromImage(img);
                }
                else {
                    qWarning() << "时间序列图读取失败:" << fullPath;
                }
            }
        }
    }

    return true;
}

// 极速查找函数
const BrainRegion* BrainNetworkData::regionByRank(int rank) const
{
    return m_rankHash.value(rank, nullptr);
}

const BrainRegion* BrainNetworkData::regionByChinese(const QString& name) const
{
    return m_chineseHash.value(name, nullptr);
}

const BrainRegion* BrainNetworkData::regionByEnglish(const QString& name) const
{
    return m_englishHash.value(name, nullptr);
}