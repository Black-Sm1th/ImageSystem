#include "BrainMetrics.h"
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
    asegRows_.clear();
    aparcRows_.clear();
    asegBySegId_.clear();
    aparcByName_.clear();

    std::filesystem::path base(baseDir_);
    bool okAseg = LoadAseg(base / "aseg.stats");
    bool okLh = LoadAparc(base / "lh.aparc.stats", 'L');
    bool okRh = LoadAparc(base / "rh.aparc.stats", 'R');
    if (!(okAseg && okLh && okRh))
    {
        return false;
    }
    ComputeAsegAsymmetry();
    return true;
}

const AsegStatsRow* BrainMetrics::FindAsegBySegId(int segId) const
{
    auto it = asegBySegId_.find(segId);
    if (it == asegBySegId_.end()) return nullptr;
    return &asegRows_[it->second];
}

const AparcStatsRow* BrainMetrics::FindAparcByName(const std::string& name) const
{
    auto it = aparcByName_.find(NormalizeName(name));
    if (it == aparcByName_.end()) return nullptr;
    return &aparcRows_[it->second];
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
        AsegStatsRow rec;
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

        const size_t idx = asegRows_.size();
        asegRows_.push_back(std::move(rec));
        if (asegRows_.back().segId >= 0)
        {
            asegBySegId_[asegRows_.back().segId] = idx;
        }
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
        AparcStatsRow rec;
        rec.hemisphere = hemisphere;
        std::string rawName;
        iss >> rawName;
        iss >> rec.numVert;      // NumVert
        iss >> rec.surfArea;     // SurfArea
        iss >> rec.grayVolMm3;   // GrayVol
        iss >> rec.thickAvg;
        iss >> rec.thickStd;
        // 曲率与折叠指标
        iss >> rec.meanCurv;
        iss >> rec.gausCurv;
        iss >> rec.foldInd;
        iss >> rec.curvInd;
        rec.baseName = rawName;
        // 按要求：lh 加 ctx-lh- 前缀，rh 加 ctx-rh- 前缀，便于与 tsv / RegionEntry 统一
        if (hemisphere == 'L')
        {
            rec.name = "ctx-lh-" + rawName;
        }
        else if (hemisphere == 'R')
        {
            rec.name = "ctx-rh-" + rawName;
        }
        else
        {
            rec.name = rawName;
        }

        const size_t idx = aparcRows_.size();
        aparcRows_.push_back(std::move(rec));
        aparcByName_[NormalizeName(aparcRows_.back().name)] = idx;
    }
    return true;
}

void BrainMetrics::ComputeAsegAsymmetry()
{
    struct Pair { int li = -1; int ri = -1; };
    std::unordered_map<std::string, Pair> pairs;
    for (int i = 0; i < static_cast<int>(asegRows_.size()); ++i)
    {
        const auto& r = asegRows_[i];
        if (r.hemisphere != 'L' && r.hemisphere != 'R') continue;
        auto& p = pairs[NormalizeName(r.baseName)];
        if (r.hemisphere == 'L') p.li = i;
        else p.ri = i;
    }
    for (const auto& kv : pairs)
    {
        const auto& p = kv.second;
        if (p.li < 0 || p.ri < 0) continue;
        auto& L = asegRows_[p.li];
        auto& R = asegRows_[p.ri];
        double denom = L.volumeMm3 + R.volumeMm3;
        if (denom <= 0) continue;
        double asym = 200.0 * std::abs(L.volumeMm3 - R.volumeMm3) / denom;
        L.asymmetryIndex = asym;
        R.asymmetryIndex = asym;
        L.partnerSegId = R.segId;
        R.partnerSegId = L.segId;
    }
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

