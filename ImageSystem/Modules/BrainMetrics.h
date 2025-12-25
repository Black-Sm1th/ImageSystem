#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

// ===== 输出数据结构 =====
// 1) aseg.stats：每行一个 SegId（包含皮下/脑室等）
struct AsegStatsRow
{
    int segId{ -1 };
    std::string name;        // 例如 Left-Thalamus / Right-Putamen
    std::string baseName;    // 去除左右前缀后的名称，用于配对（例如 Thalamus）
    char hemisphere{ 'N' };  // 'L' / 'R' / 'N'

    double nVoxels{ 0.0 };
    double volumeMm3{ 0.0 };

    double normMean{ 0.0 };
    double normStd{ 0.0 };
    double normMin{ 0.0 };
    double normMax{ 0.0 };
    double normRange{ 0.0 };

    // 200 * |L-R| / (L+R)，仅对左右成对计算
    double asymmetryIndex{ 0.0 };
    int partnerSegId{ -1 };
};

// 2) lh.aparc.stats + rh.aparc.stats 合并：每行一个皮层区（bankssts 等）
struct AparcStatsRow
{
    std::string name;        // 按要求：lh 加 ctx-lh- 前缀，rh 加 ctx-rh- 前缀
    std::string baseName;    // 原始结构名（例如 bankssts）
    char hemisphere{ 'L' };  // 'L' / 'R'

    double numVert{ 0.0 };
    double surfArea{ 0.0 };
    double grayVolMm3{ 0.0 };
    double thickAvg{ 0.0 };
    double thickStd{ 0.0 };
    double meanCurv{ 0.0 };
    double gausCurv{ 0.0 };
    double foldInd{ 0.0 };
    double curvInd{ 0.0 };
};

class BrainMetrics
{
public:
    explicit BrainMetrics(std::string baseDir);

    // 读取 baseDir 下的 aseg.stats / lh.aparc.stats / rh.aparc.stats
    bool Load();

    // 读取结果
    const std::vector<AsegStatsRow>& Aseg() const { return asegRows_; }
    const std::vector<AparcStatsRow>& Aparc() const { return aparcRows_; }

    // 查找（可选）
    const AsegStatsRow* FindAsegBySegId(int segId) const;
    const AparcStatsRow* FindAparcByName(const std::string& name) const; // name 带 ctx- 前缀

    // 工具：名称归一化 / 去前缀基名（外部匹配用）
    static std::string NormalizeName(const std::string& name);
    static std::string BaseNameFromStruct(const std::string& name, char hemisphere);

private:
    bool LoadAseg(const std::filesystem::path& filePath);
    bool LoadAparc(const std::filesystem::path& filePath, char hemisphere);
    void ComputeAsegAsymmetry();

    std::string baseDir_;
    std::vector<AsegStatsRow> asegRows_;
    std::vector<AparcStatsRow> aparcRows_; // lh+rh 合并

    std::unordered_map<int, size_t> asegBySegId_;
    std::unordered_map<std::string, size_t> aparcByName_; // normalize(name)
};

