#include "BrainMetrics.h"
#include "BrainRegionVisualizer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
    std::string ToLower(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    }

    std::string TrimLocal(const std::string& s)
    {
        const auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
        {
            return {};
        }
        const auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
}

BrainMetrics::BrainMetrics(std::string baseDir)
    : baseDir_(std::move(baseDir))
{
}

bool BrainMetrics::Load()
{
    asegRecords_.clear();
    lhAparcRecords_.clear();
    rhAparcRecords_.clear();
    bySegId_.clear();
    byNameHemi_.clear();

    std::filesystem::path base(baseDir_);
    bool okAseg = LoadAseg(base / "aseg.stats");
    bool okLh = LoadAparc(base / "lh.aparc.stats", 'L');
    bool okRh = LoadAparc(base / "rh.aparc.stats", 'R');
    if (okAseg && okLh && okRh)
    {
        ComputeAsymmetry();
        return true;
    }
    return false;
}

const BrainStatsRecord* BrainMetrics::FindBySegId(int segId) const
{
    auto it = bySegId_.find(segId);
    if (it == bySegId_.end())
    {
        return nullptr;
    }
    return Resolve(it->second);
}

const BrainStatsRecord* BrainMetrics::FindByName(const std::string& name, char hemisphere) const
{
    std::string key = NormalizeName(name) + static_cast<char>(std::toupper(static_cast<unsigned char>(hemisphere)));
    auto it = byNameHemi_.find(key);
    if (it != byNameHemi_.end())
    {
        return Resolve(it->second);
    }
    // 尝试基于去前缀的 baseName 匹配（如 ctx-lh- -> bankssts）
    std::string base = BaseNameFromStruct(name, hemisphere);
    std::string keyBase = NormalizeName(base) + static_cast<char>(std::toupper(static_cast<unsigned char>(hemisphere)));
    auto itb = byBaseNameHemi_.find(keyBase);
    if (itb != byBaseNameHemi_.end())
    {
        return Resolve(itb->second);
    }
    return nullptr;
}

const BrainStatsRecord* BrainMetrics::Resolve(const RecordRef& ref) const
{
    switch (ref.source)
    {
    case BrainStatsRecord::Source::Aseg:
        return (ref.index < asegRecords_.size()) ? &asegRecords_[ref.index] : nullptr;
    case BrainStatsRecord::Source::LH_Aparc:
        return (ref.index < lhAparcRecords_.size()) ? &lhAparcRecords_[ref.index] : nullptr;
    case BrainStatsRecord::Source::RH_Aparc:
        return (ref.index < rhAparcRecords_.size()) ? &rhAparcRecords_[ref.index] : nullptr;
    default:
        return nullptr;
    }
}

BrainStatsRecord* BrainMetrics::Resolve(const RecordRef& ref)
{
    return const_cast<BrainStatsRecord*>(static_cast<const BrainMetrics*>(this)->Resolve(ref));
}

void BrainMetrics::ApplyToRegions(std::vector<RegionEntry>& regions) const
{
    // 按需求不再修改 RegionEntry
}

bool BrainMetrics::LoadAseg(const std::filesystem::path& filePath)
{
    std::ifstream fin(filePath);
    if (!fin)
    {
        return false;
    }
    std::string line;
    while (std::getline(fin, line))
    {
        line = TrimLocal(line);
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::istringstream iss(line);
        BrainStatsRecord rec;
        rec.source = BrainStatsRecord::Source::Aseg;
        rec.hemisphere = 'N';
        int index = 0;
        iss >> index;
        iss >> rec.segId;
        iss >> rec.nVoxels;
        iss >> rec.volumeMm3;
        iss >> rec.name;
        iss >> rec.normMean;
        iss >> rec.normStd;
        iss >> rec.normMin;
        iss >> rec.normMax;
        iss >> rec.normRange;
        if (rec.name.rfind("Left-", 0) == 0)
        {
            rec.hemisphere = 'L';
            rec.baseName = rec.name.substr(5);
        }
        else if (rec.name.rfind("Right-", 0) == 0)
        {
            rec.hemisphere = 'R';
            rec.baseName = rec.name.substr(6);
        }
        else
        {
            rec.baseName = rec.name;
        }
        AddRecord(rec);
    }
    return true;
}

bool BrainMetrics::LoadAparc(const std::filesystem::path& filePath, char hemisphere)
{
    std::ifstream fin(filePath);
    if (!fin)
    {
        return false;
    }
    std::string line;
    while (std::getline(fin, line))
    {
        line = TrimLocal(line);
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::istringstream iss(line);
        BrainStatsRecord rec;
        rec.source = (hemisphere == 'L') ? BrainStatsRecord::Source::LH_Aparc : BrainStatsRecord::Source::RH_Aparc;
        rec.hemisphere = hemisphere;
        iss >> rec.name;
        iss >> rec.nVoxels;      // NumVert
        iss >> rec.surfaceArea;  // SurfArea
        iss >> rec.volumeMm3;    // GrayVol
        iss >> rec.meanThickness;
        iss >> rec.thicknessStd;
        // 曲率与折叠指标
        iss >> rec.meanCurv;
        iss >> rec.gausCurv;
        iss >> rec.foldInd;
        iss >> rec.curvInd;
        rec.baseName = rec.name;
        AddRecord(rec);
    }
    return true;
}

BrainStatsRecord* BrainMetrics::AddRecord(BrainStatsRecord record)
{
    std::vector<BrainStatsRecord>* container = nullptr;
    switch (record.source)
    {
    case BrainStatsRecord::Source::Aseg: container = &asegRecords_; break;
    case BrainStatsRecord::Source::LH_Aparc: container = &lhAparcRecords_; break;
    case BrainStatsRecord::Source::RH_Aparc: container = &rhAparcRecords_; break;
    }
    if (!container)
    {
        return nullptr;
    }

    const size_t idx = container->size();
    container->push_back(std::move(record));
    const auto& rec = container->back();

    if (rec.segId >= 0)
    {
        bySegId_[rec.segId] = { rec.source, idx };
    }
    std::string key = NormalizeName(rec.name) + rec.hemisphere;
    byNameHemi_[key] = { rec.source, idx };
    if (!rec.baseName.empty())
    {
        std::string keyBase = NormalizeName(rec.baseName) + rec.hemisphere;
        byBaseNameHemi_[keyBase] = { rec.source, idx };
    }
    return const_cast<BrainStatsRecord*>(&container->back());
}

std::string BrainMetrics::NormalizeName(const std::string& name)
{
    return ToLower(TrimLocal(name));
}

std::string BrainMetrics::BaseNameFromStruct(const std::string& name, char hemisphere)
{
    std::string trimmed = TrimLocal(name);
    auto lower = ToLower(trimmed);

    auto stripPrefix = [&](const std::string& prefix) -> bool
    {
        if (lower.rfind(prefix, 0) == 0 && trimmed.size() > prefix.size())
        {
            trimmed = trimmed.substr(prefix.size());
            lower = lower.substr(prefix.size());
            return true;
        }
        return false;
    };

    // 常见前缀：Left-/Right-，ctx-lh-/ctx-rh-/wm-lh-/wm-rh-/lh./rh./lh-/rh_
    stripPrefix("left-") || stripPrefix("right-");
    stripPrefix("ctx-lh-") || stripPrefix("ctx-rh-") || stripPrefix("wm-lh-") || stripPrefix("wm-rh-");
    stripPrefix("lh.") || stripPrefix("rh.") || stripPrefix("lh-") || stripPrefix("rh-") || stripPrefix("lh_") || stripPrefix("rh_");

    // 去除残留的分隔符
    while (!trimmed.empty() && (trimmed[0] == '-' || trimmed[0] == '_' || trimmed[0] == '.'))
    {
        trimmed.erase(0, 1);
    }
    return trimmed;
}

void BrainMetrics::ComputeAsymmetry()
{
    // 汇总左右记录
    struct Pair
    {
        RecordRef left{ BrainStatsRecord::Source::Aseg, static_cast<size_t>(-1) };
        RecordRef right{ BrainStatsRecord::Source::Aseg, static_cast<size_t>(-1) };
        bool hasLeft{ false };
        bool hasRight{ false };
    };
    std::unordered_map<std::string, Pair> pairs;

    auto collect = [&](const std::vector<BrainStatsRecord>& vec, BrainStatsRecord::Source src)
    {
        for (size_t i = 0; i < vec.size(); ++i)
        {
            const auto& rec = vec[i];
            if (rec.hemisphere != 'L' && rec.hemisphere != 'R')
            {
                continue;
            }
            auto& p = pairs[rec.baseName];
            if (rec.hemisphere == 'L')
            {
                p.left = { src, i };
                p.hasLeft = true;
            }
            else
            {
                p.right = { src, i };
                p.hasRight = true;
            }
        }
    };

    collect(asegRecords_, BrainStatsRecord::Source::Aseg);
    collect(lhAparcRecords_, BrainStatsRecord::Source::LH_Aparc);
    collect(rhAparcRecords_, BrainStatsRecord::Source::RH_Aparc);

    for (const auto& kv : pairs)
    {
        const auto& p = kv.second;
        if (!p.hasLeft || !p.hasRight)
        {
            continue;
        }
        auto* left = Resolve(p.left);
        auto* right = Resolve(p.right);
        if (!left || !right)
        {
            continue;
        }
        double denom = left->volumeMm3 + right->volumeMm3;
        if (denom <= 0.0)
        {
            continue;
        }
        double asym = 200.0 * std::abs(left->volumeMm3 - right->volumeMm3) / denom;
        left->asymmetryIndex = asym;
        right->asymmetryIndex = asym;
        left->partnerSegId = right->segId;
        right->partnerSegId = left->segId;
    }
}

