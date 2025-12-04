#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QPixmap>
#include <QHash>
#include <QUrl>

struct BrainRegion {
    int         rank;                    // 1~116
    QString     chineseName;
    QString     englishName;
    int         degree;
    double      clustering;
    double      localEfficiency;
    double      alff;
    QString     timeSeriesImagePath;     // 相对路径，如 "region_plots/001_Precentral_L.png"
    QPixmap     timeSeriesPixmap;        // 可选预加载图像

    // 用于调试输出
    QString toString() const {
        return QString("%1 (%2) - 度:%3 聚类: %4 局部效率: %5 ALFF: %6")
            .arg(chineseName).arg(englishName)
            .arg(degree).arg(clustering, 0, 'f', 4)
            .arg(localEfficiency, 0, 'f', 4).arg(alff, 0, 'f', 4);
    }
};

class BrainNetworkData : public QObject
{
    Q_OBJECT

public:
    explicit BrainNetworkData(QObject* parent = nullptr);
    ~BrainNetworkData();

    // 加载整个结果文件夹
    bool loadFromFolder(const QString& folderPath);

    // 极速查找（哈希表 O(1)）
    const BrainRegion* regionByRank(int rank) const;                    // 最快最常用
    const BrainRegion* regionByChinese(const QString& name) const;
    const BrainRegion* regionByEnglish(const QString& name) const;

    // 全部数据
    const QVector<BrainRegion>& allRegions() const { return m_regions; }

    // 主要图像
    const QPixmap& covarianceImage() const { return m_covariancePixmap; }
    const QPixmap& alffImage() const { return m_alffPixmap; }
    const QString& connectomeHtml() const { return m_connectomeHtmlPath; }

    int     regionCount()        const { return m_regions.size(); }
    double  averageDegree()      const { return m_avgDegree; }
    //平均聚类系数
    double  averageClustering()  const { return m_avgClustering; }
    //平均局部效率
    double  averageLocalEff()    const { return m_avgLocalEff; }
    double  averageALFF()        const { return m_avgALFF; }
    //全局效率
    double  globalEfficiency()   const { return m_globalEfficiency; }
    //富俱乐部连接
    double  richClubPercentage() const { return m_richClubPercentage; }
    //桥接连接
    double  bridgePercentage()   const { return m_bridgePercentage; }
    //局部连接
    double  localPercentage()    const { return m_localPercentage; }

signals:
    void loadProgress(int percent);                 // 0~100
    void loadFinished(bool success);
    void errorOccurred(const QString& msg);

private:
    bool loadJson(const QString& jsonPath);
    bool loadImages(const QString& folderPath);

    QVector<BrainRegion> m_regions;

    // 极速哈希查找表（比 QMap 快 4~6 倍）
    QHash<int, const BrainRegion*> m_rankHash;
    QHash<QString, const BrainRegion*> m_chineseHash;
    QHash<QString, const BrainRegion*> m_englishHash;

    QPixmap m_covariancePixmap;
    QPixmap m_alffPixmap;
    QString m_connectomeHtmlPath;   // file:/// 格式的本地路径

    // 缓存统计值
    double m_avgDegree = 0.0;
    double m_avgClustering = 0.0;
    double m_avgLocalEff = 0.0;
    double m_avgALFF = 0.0;
    double m_globalEfficiency = 0.0;
    double m_richClubPercentage = 0.0;
    double m_bridgePercentage = 0.0;
    double m_localPercentage = 0.0;
};
