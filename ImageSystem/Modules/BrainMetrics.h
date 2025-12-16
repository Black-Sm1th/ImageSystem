#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <optional>

struct RegionEntry;

struct BrainStatsRecord
{
    enum class Source
    {
        Aseg,
        LH_Aparc,
        RH_Aparc
    };

    Source source{ Source::Aseg };
    int segId{ -1 };
    std::string name;        // 原始结构名（例如 Left-Thalamus 或 bankssts）
    std::string baseName;    // 去除左右前缀后的名称，用于配对
    char hemisphere{ 'N' };  // 'L' / 'R' / 'N'
    double nVoxels{ 0.0 };
    double volumeMm3{ 0.0 };
    double surfaceArea{ 0.0 };   // aparc 专用
    double meanThickness{ 0.0 }; // aparc 专用
    double thicknessStd{ 0.0 };  // aparc 专用
    double normMean{ 0.0 };      // aseg intensity mean
    double normStd{ 0.0 };       // aseg intensity std
    double normMin{ 0.0 };       // aseg intensity min
    double normMax{ 0.0 };       // aseg intensity max
    double normRange{ 0.0 };     // aseg intensity range

    double meanCurv{ 0.0 };      // aparc MeanCurv
    double gausCurv{ 0.0 };      // aparc GausCurv
    double foldInd{ 0.0 };       // aparc FoldInd
    double curvInd{ 0.0 };       // aparc CurvInd

    double asymmetryIndex{ 0.0 }; // 200 * |L-R| / (L+R)，仅对左右成对计算
    int partnerSegId{ -1 };
};

class BrainMetrics
{
public:
    explicit BrainMetrics(std::string baseDir);

    // 读取 baseDir 下的 aseg.stats / lh.aparc.stats / rh.aparc.stats
    bool Load();

    const BrainStatsRecord* FindBySegId(int segId) const;
    const BrainStatsRecord* FindByName(const std::string& name, char hemisphere) const;

    // 当前实现不对 RegionEntry 做任何修改（保留接口兼容）。
    void ApplyToRegions(std::vector<RegionEntry>& regions) const;

private:
    struct RecordRef
    {
        BrainStatsRecord::Source source;
        size_t index;
    };

    const BrainStatsRecord* Resolve(const RecordRef& ref) const;
    BrainStatsRecord* Resolve(const RecordRef& ref);

    bool LoadAseg(const std::filesystem::path& filePath);
    bool LoadAparc(const std::filesystem::path& filePath, char hemisphere);
    BrainStatsRecord* AddRecord(BrainStatsRecord record);
    void ComputeAsymmetry();

    static std::string NormalizeName(const std::string& name);
    static std::string BaseNameFromStruct(const std::string& name, char hemisphere);

    std::string baseDir_;
    std::vector<BrainStatsRecord> asegRecords_;
    std::vector<BrainStatsRecord> lhAparcRecords_;
    std::vector<BrainStatsRecord> rhAparcRecords_;
    std::unordered_map<int, RecordRef> bySegId_;          // 仅 Aseg 有 SegId
    std::unordered_map<std::string, RecordRef> byNameHemi_; // key: normalize(name)+hemi
};

